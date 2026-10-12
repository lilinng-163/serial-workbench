"""Optional data instruments. Each instance reads the shared channel independently."""

import pyqtgraph as pg
from PySide6.QtWidgets import QVBoxLayout, QWidget, QTreeWidget, QTreeWidgetItem

from .theme import PALETTES
from .widgets import CheckBox, SpinBox, button, label, row


class WaveformWidget(QWidget):
    kind = "waveform"

    def __init__(self, pane):
        super().__init__()
        self.pane = pane
        self.curves = {}
        self.colors = []
        body = QVBoxLayout(self)
        self.freeze = CheckBox("冻结")
        self.count = SpinBox()
        self.count.setRange(100, 5000)
        self.count.setValue(1000)
        self.count.setSuffix(" 点")
        self.count.setMaximumWidth(125)
        body.addWidget(row(self.freeze, self.count, button("自动缩放", self.autoscale), button("数据格式", pane.window.show_wave_help)))
        self.plot = pg.PlotWidget()
        self.plot.showGrid(x=True, y=True, alpha=0.15)
        self.plot.addLegend(offset=(12, 12))
        self.plot.setLabel("bottom", "采样序号")
        self.plot.setLabel("left", "数值")
        body.addWidget(self.plot, 1)
        self.info = label("等待数值数据 · 支持 CSV、命名数值和 JSON", "subtitle")
        body.addWidget(self.info)

    def autoscale(self):
        self.plot.enableAutoRange()

    def apply_appearance(self, font, theme):
        colors = PALETTES[theme]
        self.plot.setBackground(colors["monitor"])
        for name in ("left", "bottom"):
            axis = self.plot.getAxis(name)
            axis.setPen(pg.mkPen(colors["muted"]))
            axis.setTextPen(pg.mkPen(colors["muted"]))
        legend = self.plot.getPlotItem().legend
        legend.setLabelTextColor(colors["text"])
        legend.setLabelTextSize("9pt")
        for sample, text in legend.items:
            text.setText(text.text, color=colors["text"], size="9pt")
        self.colors = [colors["rx"], colors["tx"], colors["status"], "#a775cd", "#d97592", "#829f45"]
        for index, curve in enumerate(self.curves.values()):
            curve.setPen(pg.mkPen(self.colors[index % len(self.colors)], width=2))

    def tick(self):
        if self.freeze.isChecked() or not self.pane.channel_id:
            return
        result = self.pane.engine.waveform(self.pane.channel_id, self.count.value())
        data = {}
        rejected = count = 0
        for source, batch in result["sources"].items():
            rejected += batch["rejected_lines"]
            count += batch["total_samples"]
            for sample in batch["samples"]:
                for key, value in sample["values"].items():
                    name = key if source == "default" else f"{source}/{key}"
                    xs, ys = data.setdefault(name, ([], []))
                    xs.append(sample["index"])
                    ys.append(value)
        visible = list(data)[:16]
        for name in list(self.curves):
            if name not in visible:
                self.plot.removeItem(self.curves.pop(name))
        for name in visible:
            if name not in self.curves:
                color = self.colors[len(self.curves) % len(self.colors)]
                self.curves[name] = self.plot.plot(name=name, pen=pg.mkPen(color, width=2))
            self.curves[name].setData(*data[name])
        self.info.setText(f"{count:,} 个采样 · {len(visible)} 条曲线 · 忽略 {rejected} 行")

    def settings(self):
        return {"kind": self.kind, "points": self.count.value()}

    def restore(self, settings):
        self.count.setValue(int(settings.get("points", 1000)))


class ValuesWidget(QWidget):
    kind = "values"

    def __init__(self, pane):
        super().__init__()
        self.pane = pane
        body = QVBoxLayout(self)
        self.freeze = CheckBox("冻结")
        body.addWidget(row(label("最近一次采样", "section"), self.freeze))
        self.table = QTreeWidget()
        self.table.setRootIsDecorated(False)
        self.table.setHeaderLabels(["来源", "变量", "当前值", "采样序号"])
        body.addWidget(self.table, 1)
        body.addWidget(label("从数值报文自动提取变量；与收发页、波形页共享同一连接", "subtitle"))
        self.signature = None

    def apply_appearance(self, font, theme):
        self.table.setFont(font)

    def tick(self):
        if self.freeze.isChecked() or not self.pane.channel_id:
            return
        result = self.pane.engine.waveform(self.pane.channel_id, 1)
        rows = []
        for source, batch in result["sources"].items():
            for sample in batch["samples"]:
                rows.extend((source, key, str(value), str(sample["index"])) for key, value in sample["values"].items())
        if rows != self.signature:
            self.table.clear()
            for values in rows:
                self.table.addTopLevelItem(QTreeWidgetItem(list(values)))
            self.signature = rows

    def settings(self):
        return {"kind": self.kind}

    def restore(self, settings):
        pass


# New instruments only need a QWidget factory and the same small lifecycle API.
INSTRUMENTS = {
    "waveform": ("图表", "数值波形", WaveformWidget),
    "values": ("数据", "实时数值", ValuesWidget),
}
