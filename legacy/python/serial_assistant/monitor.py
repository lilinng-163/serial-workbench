"""Readable log lines independent of serial/TCP receive chunk boundaries."""

import codecs
from dataclasses import dataclass
from itertools import groupby

from .ansi import StyledSpan, TextStyle, sgr_style


@dataclass
class MonitorLine:
    number: int
    event: dict
    text: str
    complete: bool
    spans: tuple = ()


class MonitorStream:
    """Incremental text decoding, ANSI styling and CR/LF normalization.

    Only the display is transformed. The journal retains the exact wire bytes.
    Partial lines are returned for in-place updates, so prompts need no newline.
    """

    def __init__(self, encoding="utf-8", max_line=8192):
        self.decoder = codecs.getincrementaldecoder(encoding)("replace")
        self.max_line = max_line
        self.characters = []
        self.styles = []
        self.style = TextStyle()
        self.csi = ""
        self.event = None
        self.number = 0
        self.after_cr = False
        self.escape = ""
        self.escape_length = 0

    def visible(self, character):
        if not self.escape:
            if character == "\x1b":
                self.escape = "esc"
            elif character == "\x9b":
                self.escape = "csi"
                self.csi = ""
            else:
                return character
            self.escape_length = 0
        else:
            self.escape_length += 1
            if self.escape == "esc":
                if character == "[":
                    self.escape = "csi"
                    self.csi = ""
                elif character == "]":
                    self.escape = "string"
                elif character in "P_X^":
                    self.escape = "string"
                elif not " " <= character <= "/":
                    self.escape = ""
            elif self.escape == "csi":
                if "@" <= character <= "~":
                    if character == "m":
                        self.style = sgr_style(self.style, self.csi)
                    self.escape = ""
                else:
                    self.csi += character
            elif self.escape == "string":
                if character == "\x07":
                    self.escape = ""
                elif character == "\x1b":
                    self.escape = "string_esc"
            elif self.escape == "string_esc":
                self.escape = "" if character == "\\" else "string"
            # Malformed control sequences must not hide an unlimited log stream.
            if character in "\r\n" or self.escape_length >= 1024:
                self.escape = ""
                if character in "\r\n":
                    return character
        return ""

    def finish(self, updates):
        updates.append(self.line(complete=True))
        self.characters.clear()
        self.styles.clear()
        self.event = None
        self.number += 1

    def line(self, complete):
        spans = tuple(
            StyledSpan("".join(character for character, _ in group), style)
            for style, group in groupby(zip(self.characters, self.styles), key=lambda item: item[1])
        )
        return MonitorLine(self.number, self.event, "".join(self.characters), complete, spans)

    def feed(self, data, event):
        updates = []
        for character in self.decoder.decode(data):
            if self.after_cr:
                self.after_cr = False
                if character == "\n":
                    continue
            if self.event is None:
                self.event = event
            character = self.visible(character)
            if not character:
                continue
            if character in "\r\n":
                self.finish(updates)
                self.after_cr = character == "\r"
            elif character == "\b":
                if self.characters:
                    self.characters.pop()
                    self.styles.pop()
            elif character != "\x07":
                if ord(character) < 32 and character != "\t":
                    character = chr(0x2400 + ord(character))
                self.characters.append(character)
                self.styles.append(self.style)
                if len(self.characters) >= self.max_line:
                    self.finish(updates)
        if self.characters:
            updates.append(self.line(complete=False))
        return updates
