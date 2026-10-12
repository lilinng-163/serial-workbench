"""Painted controls with legible indicators and short, interruptible motion."""

from PySide6.QtCore import QEasingCurve, QPointF, Property, QPropertyAnimation, QSize, Qt, Signal
from PySide6.QtGui import QColor, QPainter, QPalette, QPen, QPolygonF
from PySide6.QtWidgets import QAbstractButton, QCheckBox, QComboBox, QHBoxLayout, QLabel, QPlainTextEdit, QPushButton, QSpinBox, QSplitter, QSplitterHandle, QStyle, QStyleOptionButton, QWidget


class ResizeHandle(QSplitterHandle):
    def __init__(self, orientation, parent):
        super().__init__(orientation, parent)
        self.setToolTip("拖动调整大小，双击均分")
        self.setAccessibleName("可拖动分隔条")

    def enterEvent(self, event):
        super().enterEvent(event)
        self.update()

    def leaveEvent(self, event):
        super().leaveEvent(event)
        self.update()

    def paintEvent(self, event):
        painter = QPainter(self)
        painter.setRenderHint(QPainter.RenderHint.Antialiasing)
        painter.fillRect(self.rect(), self.palette().color(QPalette.ColorRole.Window))
        role = QPalette.ColorRole.Highlight if self.underMouse() else QPalette.ColorRole.Mid
        color = self.palette().color(role)
        painter.setPen(QPen(color, 3, Qt.PenStyle.SolidLine, Qt.PenCapStyle.RoundCap))
        center = self.rect().center()
        if self.orientation() == Qt.Orientation.Vertical:
            painter.drawLine(center.x() - 19, center.y(), center.x() + 19, center.y())
        else:
            painter.drawLine(center.x(), center.y() - 19, center.x(), center.y() + 19)

    def mouseDoubleClickEvent(self, event):
        if event.button() == Qt.MouseButton.LeftButton:
            splitter = self.splitter()
            visible = [i for i in range(splitter.count()) if splitter.widget(i).isVisible()]
            extent = max(1, sum(splitter.sizes()) // max(1, len(visible)))
            sizes = [extent if i in visible else 0 for i in range(splitter.count())]
            splitter.setSizes(sizes)
            splitter.splitterMoved.emit(splitter.sizes()[0], 1)
            event.accept()
            return
        super().mouseDoubleClickEvent(event)


class ResizableSplitter(QSplitter):
    def __init__(self, orientation=Qt.Orientation.Horizontal, parent=None):
        super().__init__(orientation, parent)
        self.setHandleWidth(10)
        self.setOpaqueResize(True)
        self.setChildrenCollapsible(False)

    def createHandle(self):
        return ResizeHandle(self.orientation(), self)


class FontZoomMixin:
    def wheelEvent(self, event):
        if event.modifiers() & Qt.KeyboardModifier.ControlModifier:
            delta = event.angleDelta().y()
            if not delta:
                delta = event.pixelDelta().y() * 8
            self._zoom_delta = getattr(self, "_zoom_delta", 0) + delta
            steps = int(self._zoom_delta / 120)
            self._zoom_delta -= steps * 120
            if steps:
                self.zoom_requested.emit(steps)
            event.accept()
            return
        self._zoom_delta = 0
        super().wheelEvent(event)


class ZoomPlainTextEdit(FontZoomMixin, QPlainTextEdit):
    zoom_requested = Signal(int)


class IconButton(QAbstractButton):
    def __init__(self, icon, title, parent=None):
        super().__init__(parent)
        self.icon = icon
        self.setFixedSize(36, 36)
        self.setToolTip(title)
        self.setAccessibleName(title)
        self.setCursor(Qt.CursorShape.PointingHandCursor)
        self.setFocusPolicy(Qt.FocusPolicy.StrongFocus)

    def paintEvent(self, event):
        painter = QPainter(self)
        painter.setRenderHint(QPainter.RenderHint.Antialiasing)
        if self.underMouse() or self.hasFocus():
            painter.setPen(Qt.PenStyle.NoPen)
            painter.setBrush(self.palette().color(QPalette.ColorRole.Button))
            painter.drawRoundedRect(self.rect().adjusted(1, 1, -1, -1), 6, 6)
        painter.translate(self.rect().center())
        painter.setPen(QPen(self.palette().color(QPalette.ColorRole.Text), 1.7, Qt.PenStyle.SolidLine, Qt.PenCapStyle.RoundCap))
        painter.setBrush(Qt.BrushStyle.NoBrush)
        if self.icon == "plus":
            painter.drawLine(-7, 0, 7, 0)
            painter.drawLine(0, -7, 0, 7)
        else:
            painter.drawEllipse(QPointF(0, 0), 7, 7)
            painter.drawEllipse(QPointF(0, 0), 2.5, 2.5)
            for index in range(8):
                painter.drawLine(0, -7, 0, -10)
                painter.rotate(45)


class CloseTabButton(QAbstractButton):
    def __init__(self, parent=None):
        super().__init__(parent)
        self.setFixedSize(20, 20)
        self.setToolTip("关闭连接")
        self.setCursor(Qt.CursorShape.PointingHandCursor)

    def paintEvent(self, event):
        painter = QPainter(self)
        painter.setRenderHint(QPainter.RenderHint.Antialiasing)
        color = self.palette().color(QPalette.ColorRole.Text)
        if self.underMouse():
            painter.setPen(Qt.PenStyle.NoPen)
            painter.setBrush(self.palette().color(QPalette.ColorRole.Highlight))
            painter.drawRoundedRect(self.rect().adjusted(1, 1, -1, -1), 4, 4)
            color = self.palette().color(QPalette.ColorRole.HighlightedText)
        painter.setPen(QPen(color, 1.5, Qt.PenStyle.SolidLine, Qt.PenCapStyle.RoundCap))
        painter.drawLine(6, 6, 14, 14)
        painter.drawLine(14, 6, 6, 14)


def chevron(painter, center, color, upward=False):
    painter.setPen(QPen(QColor(color), 1.8, Qt.PenStyle.SolidLine, Qt.PenCapStyle.RoundCap, Qt.PenJoinStyle.RoundJoin))
    sign = -1 if upward else 1
    painter.drawPolyline(QPolygonF([
        center + QPointF(-4, -2 * sign),
        center + QPointF(0, 2 * sign),
        center + QPointF(4, -2 * sign),
    ]))


class ToggleSwitch(QAbstractButton):
    def __init__(self, text="", parent=None):
        super().__init__(parent)
        self.setText(text)
        self.setCheckable(True)
        self.setCursor(Qt.CursorShape.PointingHandCursor)
        self.setFocusPolicy(Qt.FocusPolicy.StrongFocus)
        self._position = 0.0
        self.track_off = "#8496ad"
        self.track_on = "#2468a8"
        self.text_color = "#24334a"
        self.animation = QPropertyAnimation(self, b"position", self)
        self.animation.setDuration(160)
        self.animation.setEasingCurve(QEasingCurve.Type.OutCubic)
        self.toggled.connect(self.animate)

    def sizeHint(self):
        return QSize(48 + self.fontMetrics().horizontalAdvance(self.text()), 30)

    def minimumSizeHint(self):
        return self.sizeHint()

    def get_position(self):
        return self._position

    def set_position(self, value):
        self._position = value
        self.update()

    position = Property(float, get_position, set_position)

    def animate(self, checked):
        self.animation.stop()
        if not self.isVisible():
            self.set_position(1.0 if checked else 0.0)
            return
        self.animation.setStartValue(self._position)
        self.animation.setEndValue(1.0 if checked else 0.0)
        self.animation.start()

    def set_colors(self, colors):
        self.track_off = colors["muted"]
        self.track_on = colors["accent"]
        self.text_color = colors["text"]
        self.update()

    def paintEvent(self, event):
        painter = QPainter(self)
        painter.setRenderHint(QPainter.RenderHint.Antialiasing)
        painter.setOpacity(1.0 if self.isEnabled() else 0.45)
        off = QColor(self.track_off)
        on = QColor(self.track_on)
        color = QColor.fromRgbF(*[
            a + (b - a) * self._position
            for a, b in zip(off.getRgbF()[:3], on.getRgbF()[:3])
        ])
        y = (self.height() - 22) / 2
        painter.setPen(Qt.PenStyle.NoPen)
        painter.setBrush(color)
        painter.drawRoundedRect(1, y, 38, 22, 11, 11)
        painter.setBrush(QColor("#ffffff"))
        painter.drawEllipse(QPointF(12 + 16 * self._position, y + 11), 8, 8)
        painter.setPen(QColor(self.text_color))
        painter.drawText(self.rect().adjusted(48, 0, 0, 0), Qt.AlignmentFlag.AlignVCenter, self.text())
        if self.hasFocus():
            painter.setPen(QPen(QColor(self.track_on), 1.2, Qt.PenStyle.DotLine))
            painter.setBrush(Qt.BrushStyle.NoBrush)
            painter.drawRoundedRect(0, y - 2, 41, 26, 12, 12)


class ComboBox(QComboBox):
    def __init__(self, parent=None):
        super().__init__(parent)
        self.arrow_color = "#536b87"
        self._rotation = 0.0
        self.arrow_animation = QPropertyAnimation(self, b"rotation", self)
        self.arrow_animation.setDuration(160)
        self.arrow_animation.setEasingCurve(QEasingCurve.Type.OutCubic)
        self.popup_animation = None
        self.setMaxVisibleItems(12)

    def wheelEvent(self, event):
        if self.view().isVisible():
            super().wheelEvent(event)
        else:
            event.ignore()

    def get_rotation(self):
        return self._rotation

    def set_rotation(self, value):
        self._rotation = value
        self.update()

    rotation = Property(float, get_rotation, set_rotation)

    def animate_arrow(self, angle):
        self.arrow_animation.stop()
        self.arrow_animation.setStartValue(self._rotation)
        self.arrow_animation.setEndValue(angle)
        self.arrow_animation.start()

    def showPopup(self):
        super().showPopup()
        if not self.view().isVisible():
            return
        self.animate_arrow(180.0)
        popup = self.view().window()
        if self.popup_animation:
            self.popup_animation.stop()
        popup.setWindowOpacity(0.0)
        self.popup_animation = QPropertyAnimation(popup, b"windowOpacity", self)
        self.popup_animation.setDuration(140)
        self.popup_animation.setStartValue(0.0)
        self.popup_animation.setEndValue(1.0)
        self.popup_animation.setEasingCurve(QEasingCurve.Type.OutCubic)
        self.popup_animation.start()

    def hidePopup(self):
        if self.popup_animation:
            self.popup_animation.stop()
            self.view().window().setWindowOpacity(1.0)
        super().hidePopup()
        self.animate_arrow(0.0)

    def paintEvent(self, event):
        super().paintEvent(event)
        painter = QPainter(self)
        painter.setRenderHint(QPainter.RenderHint.Antialiasing)
        painter.translate(self.width() - 16, self.height() / 2)
        painter.rotate(self._rotation)
        chevron(painter, QPointF(0, 0), self.arrow_color)


class SpinBox(QSpinBox):
    def __init__(self, parent=None):
        super().__init__(parent)
        self.arrow_color = "#536b87"

    def wheelEvent(self, event):
        event.ignore()

    def paintEvent(self, event):
        super().paintEvent(event)
        painter = QPainter(self)
        painter.setRenderHint(QPainter.RenderHint.Antialiasing)
        chevron(painter, QPointF(self.width() - 12, self.height() * 0.27), self.arrow_color, upward=True)
        chevron(painter, QPointF(self.width() - 12, self.height() * 0.75), self.arrow_color)


class CheckBox(QCheckBox):
    def __init__(self, text="", parent=None):
        super().__init__(text, parent)
        self.indicator_color = "#657a92"
        self.accent_color = "#2468a8"
        self.fill_color = "#ffffff"

    def paintEvent(self, event):
        super().paintEvent(event)
        option = QStyleOptionButton()
        self.initStyleOption(option)
        rect = self.style().subElementRect(QStyle.SubElement.SE_CheckBoxIndicator, option, self).adjusted(1, 1, -1, -1)
        painter = QPainter(self)
        painter.setRenderHint(QPainter.RenderHint.Antialiasing)
        painter.setOpacity(1.0 if self.isEnabled() else 0.45)
        painter.setPen(QPen(QColor(self.indicator_color), 1.2))
        painter.setBrush(QColor(self.fill_color))
        painter.drawRoundedRect(rect, 3, 3)
        if self.isChecked():
            painter.setPen(QPen(QColor(self.accent_color), 1.8, Qt.PenStyle.SolidLine, Qt.PenCapStyle.RoundCap, Qt.PenJoinStyle.RoundJoin))
            center = QPointF(rect.center())
            painter.drawPolyline(QPolygonF([center + QPointF(-3, 0), center + QPointF(-1, 2.5), center + QPointF(3.5, -3)]))


def combo(items, current=None, editable=False):
    widget = ComboBox()
    widget.addItems(items)
    widget.setEditable(editable)
    if current is not None:
        widget.setCurrentText(str(current))
    return widget


def button(text, callback, primary=False):
    widget = QPushButton(text)
    if primary:
        widget.setObjectName("primary")
    widget.clicked.connect(callback)
    return widget


def row(*widgets):
    widget = QWidget()
    layout = QHBoxLayout(widget)
    layout.setContentsMargins(0, 0, 0, 0)
    for item in widgets:
        layout.addWidget(item)
    return widget


def label(text, name=""):
    widget = QLabel(text)
    widget.setObjectName(name)
    return widget
