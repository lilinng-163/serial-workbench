# SPDX-License-Identifier: Apache-2.0
"""Generate multi-resolution Windows icons from the original SVG using Qt.

Build-time helper only; the deployed application does not require Python.
"""
from pathlib import Path
import struct

from PySide6.QtCore import QRectF, Qt
from PySide6.QtGui import QImage, QPainter, QGuiApplication, QColor, QFont
from PySide6.QtSvg import QSvgRenderer

ROOT = Path(__file__).resolve().parents[1]
BRAND = ROOT / "resources/brand"


def main():
    app = QGuiApplication([])
    renderer = QSvgRenderer(str(BRAND / "lightning.svg"))
    if not renderer.isValid():
        raise RuntimeError("Invalid lightning SVG")

    frames = []
    sizes = (16, 20, 24, 32, 40, 48, 64, 128, 256)
    for size in sizes:
        image = QImage(size * 4, size * 4, QImage.Format_ARGB32_Premultiplied)
        image.fill(0)
        painter = QPainter(image)
        renderer.render(painter, QRectF(image.rect()))
        painter.end()
        image = image.scaled(size, size, mode=Qt.SmoothTransformation)
        image = image.convertToFormat(QImage.Format_ARGB32)
        if size == 256:
            image.save(str(BRAND / "lightning.png"))

        pixels = bytes(image.constBits())
        stride = image.bytesPerLine()
        rows = [pixels[y * stride:y * stride + size * 4] for y in range(size - 1, -1, -1)]
        mask_stride = ((size + 31) // 32) * 4
        masks = []
        for row in rows:
            mask = bytearray(mask_stride)
            for x in range(size):
                if row[x * 4 + 3] == 0:
                    mask[x // 8] |= 0x80 >> (x % 8)
            masks.append(bytes(mask))
        bitmap = struct.pack("<IiiHHIIiiII", 40, size, size * 2, 1, 32, 0, size * size * 4, 0, 0, 0, 0)
        frames.append(bitmap + b"".join(rows) + b"".join(masks))

    offset = 6 + 16 * len(frames)
    directory = []
    for size, data in zip(sizes, frames):
        directory.append(struct.pack("<BBBBHHII", size % 256, size % 256, 0, 0, 1, 32, len(data), offset))
        offset += len(data)
    (BRAND / "lightning.ico").write_bytes(struct.pack("<HHH", 0, 1, len(frames)) + b"".join(directory) + b"".join(frames))
    sidebar = QImage(164, 314, QImage.Format_RGB32)
    sidebar.fill(QColor("#193f35"))
    painter = QPainter(sidebar)
    renderer.render(painter, QRectF(30, 66, 104, 104))
    painter.setPen(QColor("#e2f5d8"))
    font = QFont("Segoe UI", 16)
    font.setPixelSize(20)
    font.setBold(True)
    painter.setFont(font)
    painter.drawText(QRectF(18, 194, 140, 56), Qt.AlignLeft, "Serial\nWorkbench")
    painter.setPen(QColor("#91cba1"))
    painter.drawLine(18, 261, 145, 261)
    painter.end()
    sidebar.save(str(BRAND / "installer-sidebar.bmp"))

    header = QImage(150, 57, QImage.Format_RGB32)
    header.fill(Qt.white)
    painter = QPainter(header)
    renderer.render(painter, QRectF(88, 3, 48, 48))
    painter.end()
    header.save(str(BRAND / "installer-header.bmp"))
    print("Generated lightning.ico (9 resolutions), preview and installer bitmaps")


if __name__ == "__main__":
    main()
