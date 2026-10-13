# SPDX-License-Identifier: Apache-2.0

"""Qt rendering for the same ANSI styles in monitor and terminal views."""

from itertools import groupby

from PySide6.QtGui import QColor, QFont, QSyntaxHighlighter, QTextCharFormat

from .ansi import TextStyle, ansi_color, terminal_color


def style_format(style, colors, theme):
    foreground = ansi_color(style.fg, theme) or colors["text"]
    background = ansi_color(style.bg, theme) or colors["monitor"]
    if style.reverse:
        foreground, background = background, foreground
    result = QTextCharFormat()
    result.setForeground(QColor(foreground))
    if style.bg is not None or style.reverse:
        result.setBackground(QColor(background))
    result.setFontWeight(QFont.Weight.Bold if style.bold else QFont.Weight.Normal)
    result.setFontItalic(style.italic)
    result.setFontUnderline(style.underline)
    return result


class TerminalHighlighter(QSyntaxHighlighter):
    def __init__(self, view):
        super().__init__(view.document())
        self.view = view

    def highlightBlock(self, text):
        view = self.view
        row = self.currentBlock().blockNumber()
        if row >= view.screen.lines:
            return
        cells = []
        for column in range(view.screen.columns):
            cell = view.screen.buffer[row][column]
            if cell.data:
                style = TextStyle(
                    fg=terminal_color(cell.fg), bg=terminal_color(cell.bg),
                    bold=cell.bold, italic=cell.italics, underline=cell.underscore, reverse=cell.reverse,
                )
                cells.append((cell.data, style))
        position = 0
        for style, group in groupby(cells, key=lambda item: item[1]):
            data = "".join(character for character, _ in group)
            length = len(data.encode("utf-16-le")) // 2
            self.setFormat(position, length, style_format(style, view.colors, view.ansi_theme))
            position += length
