"""Bounded stream extraction of JSON and XML embedded in device logs."""

import codecs
from collections import deque
from datetime import datetime, timezone
import json
import re
from xml.etree import ElementTree


class StructuredDecoder:
    MAX_PENDING = 65536
    MAX_RECORD_BYTES = 2 * 1024 * 1024
    TOKEN = re.compile(r'<!--[\s\S]*?-->|<!\[CDATA\[[\s\S]*?\]\]>|<\?[\s\S]*?\?>|</?[A-Za-z_][\w:.-]*(?:[^<>"\']|"[^"]*"|\'[^\']*\')*>')

    def __init__(self):
        self.mode = "off"
        self.encoding = "utf-8"
        self.records = deque()
        self.sequence = 0
        self.byte_count = 0
        self.rejected = 0
        self.reset()

    def reset(self):
        self.decoder = codecs.getincrementaldecoder(self.encoding)("replace")
        self.pending = ""
        self.ansi_state = "text"

    def plain_text(self, text):
        output = []
        for char in text:
            state = self.ansi_state
            if state == "text":
                if char == "\x1b":
                    self.ansi_state = "escape"
                else:
                    output.append(char)
            elif state == "escape":
                self.ansi_state = "csi" if char == "[" else "osc" if char == "]" else "text"
            elif state == "csi":
                if "@" <= char <= "~":
                    self.ansi_state = "text"
            elif state == "osc":
                if char == "\x07":
                    self.ansi_state = "text"
                elif char == "\x1b":
                    self.ansi_state = "osc_escape"
            elif state == "osc_escape":
                self.ansi_state = "text" if char == "\\" else "osc"
        return "".join(output)

    def configure(self, mode, encoding="utf-8"):
        if mode not in ("off", "auto", "json", "xml"):
            raise ValueError("解析格式必须为 off / auto / json / xml")
        codecs.lookup(encoding)
        if (mode, encoding) != (self.mode, self.encoding):
            self.mode = mode
            self.encoding = encoding
            self.reset()

    @staticmethod
    def json_end(text):
        stack = []
        quoted = False
        escaped = False
        for offset, char in enumerate(text):
            if quoted:
                if escaped:
                    escaped = False
                elif char == "\\":
                    escaped = True
                elif char == '"':
                    quoted = False
                continue
            if char == '"':
                quoted = True
            elif char in "[{":
                stack.append(char)
                if len(stack) > 128:
                    return -1
            elif char in "]}":
                if not stack or stack.pop() != ("]" == char and "[" or "{"):
                    return -1
                if not stack:
                    return offset + 1
        return None

    @classmethod
    def xml_end(cls, text):
        depth = 0
        for match in cls.TOKEN.finditer(text):
            token = match.group()
            if token.startswith(("<!", "<?")):
                continue
            if token.startswith("</"):
                depth -= 1
            elif not token.endswith("/>"):
                depth += 1
            if depth > 128:
                return -1
            if depth == 0:
                return match.end()
        return None

    def feed(self, data, event_seq, peer=""):
        if self.mode == "off":
            return
        self.pending += self.plain_text(self.decoder.decode(data))
        if len(self.pending) > self.MAX_PENDING:
            self.pending = self.pending[-self.MAX_PENDING:]
            self.rejected += 1
        pattern = r'[\[{]|<[A-Za-z_]' if self.mode == "auto" else r'[\[{]' if self.mode == "json" else r'<[A-Za-z_]'
        while self.pending:
            start = re.search(pattern, self.pending)
            if start is None:
                # Keep a split XML opener or a log prefix only briefly.
                self.pending = self.pending[-1:] if self.pending.endswith("<") else ""
                return
            self.pending = self.pending[start.start():]
            kind = "xml" if self.pending.startswith("<") else "json"
            end = self.xml_end(self.pending) if kind == "xml" else self.json_end(self.pending)
            if end is None:
                # A bracketed log label is not a JSON frame; recover at its newline.
                if kind == "json" and len(self.pending) > 1 and self.pending[1] not in '\r\n\t {[:"0123456789-]}tfn' and "\n" in self.pending:
                    self.pending = self.pending.split("\n", 1)[1]
                    self.rejected += 1
                    continue
                return
            if end == -1:
                self.pending = self.pending[1:]
                self.rejected += 1
                continue
            raw = self.pending[:end]
            self.pending = self.pending[end:]
            try:
                if kind == "json":
                    value = json.loads(raw)
                    pretty = json.dumps(value, ensure_ascii=False, indent=2)
                else:
                    if "<!DOCTYPE" in raw.upper() or "<!ENTITY" in raw.upper():
                        raise ValueError("XML declarations are unsupported")
                    root = ElementTree.fromstring(raw)
                    ElementTree.indent(root, space="  ")
                    pretty = ElementTree.tostring(root, encoding="unicode")
                    value = None
            except (ValueError, ElementTree.ParseError, RecursionError):
                self.rejected += 1
                continue
            self.sequence += 1
            record = {
                "seq": self.sequence, "event_seq": event_seq, "peer": peer,
                "time": datetime.now(timezone.utc).isoformat(timespec="milliseconds"),
                "format": kind, "text": raw, "pretty": pretty, "value": value,
            }
            size = len(json.dumps(record, ensure_ascii=False).encode("utf-8"))
            self.records.append((record, size))
            self.byte_count += size
            while len(self.records) > 500 or self.byte_count > self.MAX_RECORD_BYTES:
                _, removed = self.records.popleft()
                self.byte_count -= removed

    def read(self, after=0, limit=100):
        if after < 0 or not 1 <= limit <= 500:
            raise ValueError("after >= 0, limit 1..500")
        records = [record.copy() for record, _ in self.records if record["seq"] > after]
        page = records[:limit]
        oldest = self.records[0][0]["seq"] if self.records else self.sequence + 1
        return {
            "mode": self.mode, "encoding": self.encoding, "records": page,
            "next_cursor": page[-1]["seq"] if len(records) > limit else self.sequence,
            "gap": after < oldest - 1, "has_more": len(records) > limit,
            "rejected": self.rejected,
        }
