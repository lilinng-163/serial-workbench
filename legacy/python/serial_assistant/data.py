# SPDX-License-Identifier: Apache-2.0

"""Bounded, non-destructive event journal and incremental waveform decoding."""

from collections import deque
from datetime import datetime, timezone
import json
import math
import re
import threading
import time
import uuid


class EventJournal:
    def __init__(self, max_events=20000, max_bytes=8 * 1024 * 1024):
        self.session_id = uuid.uuid4().hex
        self.events = deque()
        self.max_events = max_events
        self.max_bytes = max_bytes
        self.byte_count = 0
        self.sequence = 0
        self.condition = threading.Condition()

    def append(self, channel_id, direction, data=b"", message="", peer="", stream_reset=False):
        with self.condition:
            self.sequence += 1
            event = {
                "seq": self.sequence,
                "time": datetime.now(timezone.utc).isoformat(timespec="milliseconds"),
                "channel_id": channel_id,
                "direction": direction,
                "hex": data.hex(" "),
                "text": data.decode("utf-8", errors="replace"),
                "size": len(data),
                "message": message,
                "peer": peer,
            }
            if stream_reset:
                event["stream_reset"] = True
            # Budget the encoded payload, not merely the raw wire bytes.
            size = len(json.dumps(event, ensure_ascii=False).encode("utf-8"))
            self.events.append((event, size))
            self.byte_count += size
            while len(self.events) > self.max_events or self.byte_count > self.max_bytes:
                _, removed_size = self.events.popleft()
                self.byte_count -= removed_size
            self.condition.notify_all()
            return event

    def read(self, after=0, limit=200, channel_id=None, wait_ms=0):
        if after < 0 or not 1 <= limit <= 1000 or not 0 <= wait_ms <= 10000:
            raise ValueError("after >= 0, limit 1..1000, wait_ms 0..10000")
        deadline = time.monotonic() + wait_ms / 1000
        with self.condition:
            while True:
                oldest = self.events[0][0]["seq"] if self.events else self.sequence + 1
                records = [
                    dict(event) for event, _ in self.events
                    if event["seq"] > after
                    and (channel_id is None or event["channel_id"] == channel_id)
                ]
                remaining = deadline - time.monotonic()
                if records or remaining <= 0 or after > self.sequence or after < oldest - 1:
                    break
                self.condition.wait(remaining)
            page = records[:limit]
            return {
                "session_id": self.session_id,
                "events": page,
                "next_cursor": page[-1]["seq"] if len(records) > limit else self.sequence,
                "latest_cursor": self.sequence,
                "oldest_cursor": oldest,
                "has_more": len(records) > limit,
                "gap": after < oldest - 1,
                "cursor_reset_required": after > self.sequence,
            }

    def snapshot(self, channel_id=None):
        with self.condition:
            return [dict(e) for e, _ in self.events if channel_id is None or e["channel_id"] == channel_id]


class WaveDecoder:
    """One decoder per source, handling arbitrary stream chunk boundaries."""

    def __init__(self):
        self.pending = bytearray()
        self.discarding = False
        self.samples = deque(maxlen=5000)
        self.counter = 0
        self.rejected = 0

    @staticmethod
    def parse(line):
        text = line.decode("utf-8").strip()
        if not text:
            return None
        if text.startswith(("{", "[")):
            parsed = json.loads(text)
            if isinstance(parsed, dict):
                values = parsed
            elif isinstance(parsed, list):
                values = {f"CH{i + 1}": value for i, value in enumerate(parsed)}
            else:
                return None
        else:
            parts = re.split(r"[,;\s]+", text)
            if all("=" in part or ":" in part for part in parts):
                values = dict(re.split("[=:]", part, maxsplit=1) for part in parts)
            else:
                values = {f"CH{i + 1}": value for i, value in enumerate(parts)}
        if not 1 <= len(values) <= 16:
            return None
        result = {}
        for key, value in values.items():
            if isinstance(value, bool) or not isinstance(value, (str, int, float)):
                return None
            number = float(value)
            if not math.isfinite(number):
                return None
            result[str(key)[:64]] = number
        return result

    def feed(self, data):
        self.pending.extend(data)
        while b"\n" in self.pending:
            line, _, tail = self.pending.partition(b"\n")
            self.pending = bytearray(tail)
            if self.discarding or len(line) > 8192:
                self.discarding = False
                self.rejected += 1
                continue
            try:
                values = self.parse(line)
            except (ValueError, UnicodeError, TypeError, OverflowError):
                values = None
            if values:
                self.counter += 1
                self.samples.append({"index": self.counter, "time": time.time(), "values": values})
            elif line.strip():
                self.rejected += 1
        if len(self.pending) > 8192:
            self.pending.clear()
            self.discarding = True

    def read(self, limit=1000):
        if not 1 <= limit <= 5000:
            raise ValueError("limit must be 1..5000")
        return {"samples": list(self.samples)[-limit:], "total_samples": self.counter, "rejected_lines": self.rejected}


def encode_payload(payload, encoding="utf-8", hex_mode=False, newline="none"):
    endings = {"none": b"", "lf": b"\n", "cr": b"\r", "crlf": b"\r\n"}
    if newline not in endings:
        raise ValueError("newline must be none, lf, cr or crlf")
    data = bytes.fromhex(payload) if hex_mode else payload.encode(encoding)
    data += endings[newline]
    if not data or len(data) > 65507:
        raise ValueError("报文长度需要为 1..65507 字节")
    return data
