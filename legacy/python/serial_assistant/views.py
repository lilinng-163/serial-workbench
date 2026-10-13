# SPDX-License-Identifier: Apache-2.0

"""Stream-aware monitor and terminal widgets."""
import codecs
import pyte
from PySide6.QtCore import Qt, QTimer, Signal
from PySide6.QtGui import QColor, QFont, QKeySequence, QPainter, QPalette, QTextBlockFormat, QTextCharFormat, QTextCursor
from PySide6.QtWidgets import QApplication, QPlainTextEdit, QTextEdit
from .monitor import MonitorStream
from .theme import PALETTES
from .text_rendering import TerminalHighlighter, style_format
from .widgets import FontZoomMixin, ZoomPlainTextEdit


class TerminalView(ZoomPlainTextEdit):
    transmit = Signal(bytes)

    def __init__(self):
        super().__init__()
        self.setObjectName("terminal")
        self.setReadOnly(True)
        self.setFont(QFont("Cascadia Mono", 11))
        self.setLineWrapMode(QPlainTextEdit.LineWrapMode.NoWrap)
        self.screen = pyte.Screen(120, 32)
        self.colors = PALETTES["light"]
        self.ansi_theme = "light"
        self.highlighter = TerminalHighlighter(self)
        self.stream = pyte.Stream(self.screen)
        self.decoder = codecs.getincrementaldecoder("utf-8")("replace")
        self.enabled_input = False
        self.dirty = True
        self.caret_on = True
        self.caret_timer = QTimer(self)
        self.caret_timer.setInterval(500)
        self.caret_timer.timeout.connect(self.blink_caret)
        self.caret_timer.start()
        self.setAttribute(Qt.WidgetAttribute.WA_InputMethodEnabled, True)

    def blink_caret(self):
        self.caret_on = not self.caret_on
        self.viewport().update()

    def focusInEvent(self, event):
        super().focusInEvent(event)
        self.caret_on = True
        self.viewport().update()

    def focusOutEvent(self, event):
        super().focusOutEvent(event)
        self.viewport().update()

    def paintEvent(self, event):
        super().paintEvent(event)
        if self.enabled_input and self.hasFocus() and self.caret_on and not self.screen.cursor.hidden:
            painter = QPainter(self.viewport())
            rect = self.cursorRect()
            rect.setWidth(max(2, self.fontMetrics().horizontalAdvance("M")))
            color = self.palette().color(QPalette.ColorRole.Text)
            color.setAlpha(140)
            painter.fillRect(rect, color)

    def focusNextPrevChild(self, next):
        return False if self.enabled_input else super().focusNextPrevChild(next)

    def inputMethodEvent(self, event):
        if self.enabled_input and event.commitString():
            self.transmit.emit(event.commitString().encode("utf-8"))
            event.accept()
        else:
            super().inputMethodEvent(event)

    def inputMethodQuery(self, query):
        if query == Qt.InputMethodQuery.ImEnabled:
            return self.enabled_input
        return super().inputMethodQuery(query)

    def reset(self, encoding="utf-8"):
        self.screen.reset()
        self.reset_stream(encoding)
        self.dirty = True
        self.render()

    def reset_stream(self, encoding="utf-8"):
        self.stream = pyte.Stream(self.screen)
        self.screen.select_graphic_rendition(0)
        self.decoder = codecs.getincrementaldecoder(encoding)("replace")

    def feed(self, data):
        self.stream.feed(self.decoder.decode(data))
        self.dirty = True

    def render(self):
        if self.dirty:
            display = self.screen.display
            self.setPlainText("\n".join(display))
            cursor = self.textCursor()
            y = self.screen.cursor.y
            x = self.screen.cursor.x
            prefix = "".join(self.screen.buffer[y][column].data for column in range(x))
            before = "".join(line + "\n" for line in display[:y]) + prefix
            position = len(before.encode("utf-16-le")) // 2
            cursor.setPosition(min(position, self.document().characterCount() - 1))
            self.setTextCursor(cursor)
            self.dirty = False

    def keyPressEvent(self, event):
        if event.matches(QKeySequence.StandardKey.Copy) and self.textCursor().hasSelection():
            return super().keyPressEvent(event)
        if not self.enabled_input:
            return super().keyPressEvent(event)
        if event.matches(QKeySequence.StandardKey.Paste):
            self.transmit.emit(QApplication.clipboard().text().encode("utf-8"))
            return
        keys = {
            Qt.Key.Key_Return: b"\r", Qt.Key.Key_Enter: b"\r", Qt.Key.Key_Backspace: b"\x7f",
            Qt.Key.Key_Tab: b"\t", Qt.Key.Key_Escape: b"\x1b", Qt.Key.Key_Up: b"\x1b[A",
            Qt.Key.Key_Down: b"\x1b[B", Qt.Key.Key_Right: b"\x1b[C", Qt.Key.Key_Left: b"\x1b[D",
            Qt.Key.Key_Home: b"\x1b[H", Qt.Key.Key_End: b"\x1b[F", Qt.Key.Key_Delete: b"\x1b[3~",
        }
        if event.modifiers() & Qt.KeyboardModifier.ControlModifier and Qt.Key.Key_A <= event.key() <= Qt.Key.Key_Z:
            data = bytes([event.key() - Qt.Key.Key_A + 1])
        else:
            data = keys.get(event.key(), event.text().encode("utf-8"))
        if data:
            self.transmit.emit(data)


class MonitorView(FontZoomMixin, QTextEdit):
    zoom_requested = Signal(int)

    def __init__(self):
        super().__init__()
        self.streams = {}
        self.partial_blocks = {}
        self.show_timestamps = True
        self.colors = PALETTES["light"]
        self.ansi_theme = "light"

    def clear(self):
        super().clear()
        self.reset_streams()

    def reset_streams(self):
        self.streams.clear()
        self.partial_blocks.clear()

    @staticmethod
    def char_format(color):
        result = QTextCharFormat()
        result.setForeground(QColor(color))
        return result

    def write_line(self, event, content, key=None, number=None, complete=True, spans=()):
        previous = self.partial_blocks.get(key)
        if previous and previous[0] == number and previous[1].isValid():
            cursor = QTextCursor(previous[1])
            cursor.movePosition(QTextCursor.MoveOperation.StartOfBlock)
            cursor.movePosition(QTextCursor.MoveOperation.EndOfBlock, QTextCursor.MoveMode.KeepAnchor)
            cursor.removeSelectedText()
        else:
            cursor = QTextCursor(self.document())
            cursor.movePosition(QTextCursor.MoveOperation.End)
            if not self.document().isEmpty():
                cursor.insertBlock()
        block_format = QTextBlockFormat()
        block_format.setTopMargin(0)
        block_format.setBottomMargin(0)
        cursor.setBlockFormat(block_format)
        direction = event["direction"]
        color = self.colors[direction]
        origin = f' [{event["peer"]}]' if event["peer"] else ""
        if direction == "tx":
            origin += f' ({event["message"]})'
        if self.show_timestamps:
            cursor.insertText(f'{event["time"][11:23]}Z ', self.char_format(self.colors["muted"]))
        cursor.insertText(f"{direction.upper()}{origin}  ", self.char_format(color))
        if spans:
            for span in spans:
                cursor.insertText(span.text, style_format(span.style, self.colors, self.ansi_theme))
        else:
            cursor.insertText(content, self.char_format(self.colors["text"]))
        if key is not None:
            if complete:
                self.partial_blocks.pop(key, None)
            else:
                self.partial_blocks[key] = (number, cursor.block())

    def feed_event(self, event, encoding, hex_mode=False, search=""):
        if event.get("stream_reset"):
            self.reset_streams()
        if event["direction"] == "status" or hex_mode:
            content = event["message"] if event["direction"] == "status" else event["hex"]
            if not search or search in (content + event["hex"]).casefold():
                self.write_line(event, content)
            return
        key = (event["direction"], event["peer"])
        if key not in self.streams:
            if len(self.streams) >= 32:
                oldest = next(iter(self.streams))
                self.streams.pop(oldest)
                self.partial_blocks.pop(oldest, None)
            self.streams[key] = MonitorStream(encoding)
        for line in self.streams[key].feed(bytes.fromhex(event["hex"]), event):
            if not search or search in (line.text + line.event["hex"] + line.event["message"]).casefold():
                self.write_line(line.event, line.text, key, line.number, line.complete, line.spans)
            elif line.complete:
                self.partial_blocks.pop(key, None)


