# SPDX-License-Identifier: Apache-2.0

"""Independent connection tabs, each with a live monitor and optional waveform."""

import json
from pathlib import Path

from PySide6.QtCore import Qt, QTimer, Signal
from PySide6.QtWidgets import (
    QFileDialog, QHBoxLayout, QLineEdit, QMenu,
    QPlainTextEdit, QStackedWidget, QTabBar, QTabWidget,
    QTreeWidget, QTreeWidgetItem, QVBoxLayout, QWidget, QToolButton,
)

from .instruments import INSTRUMENTS
from .theme import PALETTES
from .views import MonitorView, TerminalView
from .widgets import CheckBox, CloseTabButton, IconButton, ComboBox, ResizableSplitter, SpinBox, ToggleSwitch, ZoomPlainTextEdit, button, combo, label, row


STATES = {"connected": "已连接", "listening": "监听中", "connecting": "连接中", "reconnecting": "等待恢复", "closed": "已断开", "error": "错误"}


class ConnectionPane(QWidget):
    def __init__(self, window, channel_id=None):
        super().__init__()
        self.window = window
        self.engine = window.engine
        self.channel_id = channel_id
        self.cursor = 0
        self.view_start = 0
        self.pending = []
        self.repeat_job = None
        self.parsed_cursors = {}
        self.panel_sizes = [700, 160]
        self.repeat_timer = QTimer(self)
        self.repeat_timer.timeout.connect(self.repeat_send)
        self.build()

    def build(self):
        layout = QVBoxLayout(self)
        layout.setContentsMargins(8, 6, 8, 6)
        layout.setSpacing(6)
        self.instruments = []
        self.channel_title = label("新连接", "section")
        self.channel_title.hide()
        self.counters = label("尚未连接 · 在左侧选择端口并建立连接", "subtitle")
        self.data_tabs = QTabWidget()
        self.data_tabs.setDocumentMode(True)
        self.data_tabs.setTabsClosable(True)
        self.data_tabs.tabCloseRequested.connect(self.close_instrument)
        self.control_button = QToolButton()
        self.control_button.setText("控件")
        self.control_button.setToolTip("在当前连接中添加控件页")
        self.control_button.setPopupMode(QToolButton.ToolButtonPopupMode.InstantPopup)
        self.control_button.setMenu(self.instrument_menu())
        self.data_tabs.setCornerWidget(self.control_button)
        layout.addWidget(self.data_tabs, 1)
        self.monitor_panel = QWidget()
        mon = QVBoxLayout(self.monitor_panel)
        mon.setContentsMargins(0, 6, 0, 0)
        self.terminal_mode = ToggleSwitch("终端")
        self.timestamps = CheckBox("时间戳", self)
        self.timestamps.setChecked(True)
        self.timestamps.hide()
        self.hex_view = CheckBox("HEX")
        self.pause = CheckBox("暂停")
        self.follow = CheckBox("跟随", self)
        self.follow.setChecked(True)
        self.follow.hide()
        self.pause.setToolTip("暂停显示，后台继续接收")
        self.filter = QLineEdit()
        self.filter.setPlaceholderText("筛选收发内容")
        self.filter.setClearButtonEnabled(True)
        options = QToolButton()
        options.setText("更多")
        options.setPopupMode(QToolButton.ToolButtonPopupMode.InstantPopup)
        menu = QMenu(options)
        for control in (self.timestamps, self.follow):
            action = menu.addAction(control.text())
            action.setCheckable(True)
            action.setChecked(control.isChecked())
            action.toggled.connect(control.setChecked)
            control.toggled.connect(action.setChecked)
        menu.addSeparator()
        menu.addAction("导出日志…", self.export_log)
        options.setMenu(menu)
        tools = row(self.terminal_mode, self.hex_view, self.pause, self.filter, button("清屏", self.clear_view), options)
        tools.layout().setStretch(3, 1)
        mon.addWidget(tools)
        self.display_stack = QStackedWidget()
        self.console = MonitorView()
        self.console.setObjectName("console")
        self.console.setReadOnly(True)
        self.console.document().setMaximumBlockCount(4000)
        self.console.setPlaceholderText("等待连接设备…")
        self.terminal = TerminalView()
        self.terminal.transmit.connect(self.send_terminal)
        self.display_stack.addWidget(self.console)
        self.display_stack.addWidget(self.terminal)
        self.sender_panel = self.build_sender()
        self.monitor_splitter = ResizableSplitter(Qt.Orientation.Vertical)
        self.monitor_splitter.addWidget(self.display_stack)
        self.monitor_splitter.addWidget(self.sender_panel)
        self.monitor_splitter.setStretchFactor(0, 1)
        self.monitor_splitter.setSizes([700, 160])
        mon.addWidget(self.monitor_splitter, 1)
        self.data_tabs.addTab(self.monitor_panel, "收发")
        self.data_tabs.tabBar().setTabButton(0, QTabBar.ButtonPosition.RightSide, None)
        self.structured_panel = QWidget()
        structured_body = QVBoxLayout(self.structured_panel)
        self.parse_mode = combo(["不解析", "JSON / XML", "仅 JSON", "仅 XML"])
        structured_body.addWidget(row(label("解析格式"), self.parse_mode))
        structured = ResizableSplitter(Qt.Orientation.Vertical)
        self.parsed_list = QTreeWidget()
        self.parsed_list.setHeaderLabels(["时间", "格式", "来源", "内容"])
        self.parsed_list.setColumnWidth(0, 105)
        self.parsed_list.setColumnWidth(1, 65)
        self.parsed_list.setColumnWidth(2, 100)
        self.parsed_list.setRootIsDecorated(False)
        self.parsed_detail = QPlainTextEdit()
        self.parsed_detail.setReadOnly(True)
        structured.addWidget(self.parsed_list)
        structured.addWidget(self.parsed_detail)
        structured_body.addWidget(structured, 1)
        self.parsed_list.currentItemChanged.connect(self.show_parsed)
        layout.addWidget(self.counters)
        self.terminal_mode.toggled.connect(self.toggle_terminal_mode)
        self.timestamps.toggled.connect(self.timestamp_changed)
        self.hex_view.toggled.connect(self.reload_view)
        self.filter.editingFinished.connect(self.reload_view)
        self.filter.editingFinished.connect(self.filter_structured)
        self.parse_mode.currentIndexChanged.connect(self.parser_changed)
        for view in (self.console, self.terminal, self.payload):
            view.zoom_requested.connect(lambda steps: self.window.font_size.setValue(self.window.font_size.value() + steps))

    def instrument_menu(self):
        menu = QMenu(self)
        groups = {}
        for kind, (group, title, factory) in INSTRUMENTS.items():
            if group not in groups:
                groups[group] = menu.addMenu(group)
            groups[group].addAction(title, lambda checked=False, key=kind: self.add_instrument(key))
        groups["数据"].addAction("JSON / XML", lambda: self.add_instrument("structured"))
        return menu

    def add_instrument(self, kind, settings=None, activate=True):
        if kind == "structured":
            index = self.data_tabs.indexOf(self.structured_panel)
            if index < 0:
                index = self.data_tabs.addTab(self.structured_panel, "JSON / XML")
            if self.parse_mode.currentIndex() == 0:
                self.parse_mode.setCurrentIndex(1)
            widget = self.structured_panel
        elif kind in INSTRUMENTS:
            group, title, factory = INSTRUMENTS[kind]
            widget = factory(self)
            if settings:
                widget.restore(settings)
            widget.apply_appearance(self.window.display_font, self.window.theme_name)
            self.instruments.append(widget)
            number = 1
            titles = {self.data_tabs.tabText(i) for i in range(self.data_tabs.count())}
            while f"{title} {number}" in titles:
                number += 1
            index = self.data_tabs.addTab(widget, f"{title} {number}")
        else:
            return None
        close = CloseTabButton(self.data_tabs)
        close.setToolTip("关闭控件")
        close.setAccessibleName("关闭控件")
        close.clicked.connect(lambda: self.close_instrument(self.data_tabs.indexOf(widget)))
        self.data_tabs.tabBar().setTabButton(index, QTabBar.ButtonPosition.RightSide, close)
        if activate:
            self.data_tabs.setCurrentWidget(widget)
        self.window.color_controls()
        return widget

    def close_instrument(self, index):
        if index <= 0:
            return
        widget = self.data_tabs.widget(index)
        self.data_tabs.removeTab(index)
        if widget is self.structured_panel:
            self.parse_mode.setCurrentIndex(0)
        else:
            self.instruments.remove(widget)
            widget.deleteLater()

    def build_sender(self):
        panel = QWidget()
        layout = QVBoxLayout(panel)
        layout.setContentsMargins(0, 0, 0, 0)
        layout.setSpacing(4)
        self.send_hex = CheckBox("HEX")
        self.send_hex.setToolTip("将输入内容作为十六进制字节发送")
        self.encoding = combo(["utf-8", "gbk", "ascii", "latin-1"])
        self.encoding.setToolTip("文本编码，同时用于接收显示")
        self.newline = combo(["无换行", "LF", "CR", "CRLF"], "CRLF")
        self.newline.setToolTip("发送时追加的换行符，HEX 模式同样生效")
        self.peer = ComboBox()
        self.peer.setEditable(True)
        self.peer.lineEdit().setPlaceholderText("目标 IP:端口")
        self.peer.setToolTip("TCP 客户端或 UDP 发送目标")
        layout.addWidget(row(self.send_hex, self.encoding, self.newline, self.peer))
        self.payload = ZoomPlainTextEdit()
        self.payload.setMinimumHeight(44)
        self.payload.setPlaceholderText("输入待发送内容；HEX 示例：01 03 00 00")
        layout.addWidget(self.payload)
        self.template_box = combo(["报文模板"])
        self.template_box.currentIndexChanged.connect(self.apply_template)
        self.repeat = CheckBox("循环发送")
        self.interval = SpinBox()
        self.interval.setRange(20, 3600000)
        self.interval.setValue(1000)
        self.interval.setSuffix(" ms")
        self.interval.setMaximumWidth(115)
        self.interval.setToolTip("循环发送间隔")
        self.repeat.toggled.connect(self.toggle_repeat)
        self.send_button = button("发送", self.send_current, True)
        layout.addWidget(row(self.template_box, button("模板管理", self.template_menu), self.repeat, self.interval, self.send_button))
        self.encoding.currentTextChanged.connect(self.encoding_changed)
        return panel

    def appearance(self):
        sizes = self.monitor_splitter.sizes()
        if all(size > 0 for size in sizes):
            self.panel_sizes = sizes
        controls = [widget.settings() for widget in self.instruments]
        if self.data_tabs.indexOf(self.structured_panel) >= 0:
            controls.append({"kind": "structured"})
        return {
            "timestamps": self.timestamps.isChecked(), "terminal_mode": self.terminal_mode.isChecked(),
            "parser": self.parse_mode.currentIndex(), "encoding": self.encoding.currentText(),
            "instruments": controls, "sender_sizes": self.panel_sizes.copy(),
        }

    def restore(self, settings):
        while self.data_tabs.count() > 1:
            self.close_instrument(1)
        self.timestamps.setChecked(bool(settings.get("timestamps", True)))
        self.terminal_mode.setChecked(bool(settings.get("terminal_mode", False)))
        self.encoding.setCurrentText(settings.get("encoding", "utf-8"))
        # Migrate old inline-wave settings without reserving space on the monitor page.
        controls = settings.get("instruments", [])
        if "instruments" not in settings and settings.get("show_wave"):
            controls = [{"kind": "waveform"}]
        for control in controls:
            if isinstance(control, dict):
                self.add_instrument(control.get("kind"), control, activate=False)
        self.parse_mode.setCurrentIndex(max(0, min(3, int(settings.get("parser", 0)))))
        self.data_tabs.setCurrentIndex(0)
        sizes = settings.get("sender_sizes", [700, 160])
        if isinstance(sizes, list) and len(sizes) == 2 and all(isinstance(value, int) and value > 0 for value in sizes):
            self.panel_sizes = sizes.copy()
            QTimer.singleShot(0, lambda: self.monitor_splitter.setSizes(self.panel_sizes))

    def apply_appearance(self, font, theme, replay=False):
        colors = PALETTES[theme]
        for view in (self.console, self.terminal):
            view.colors = colors
            view.ansi_theme = theme
        for view in (self.console, self.terminal, self.payload, self.parsed_detail):
            view.setFont(font)
        for instrument in self.instruments:
            instrument.apply_appearance(font, theme)
        self.terminal.highlighter.rehighlight()
        if replay:
            self.reload_view()
        self.terminal.dirty = True
        self.terminal.render()


    def timestamp_changed(self, checked):
        self.console.show_timestamps = checked
        self.reload_view()

    def toggle_terminal_mode(self, checked):
        if checked:
            sizes = self.monitor_splitter.sizes()
            if all(size > 0 for size in sizes):
                self.panel_sizes = sizes
        self.repeat.setChecked(False)
        self.pause.setChecked(False)
        self.display_stack.setCurrentIndex(1 if checked else 0)
        self.terminal.enabled_input = checked and self.channel_id is not None
        self.sender_panel.setVisible(not checked)
        self.hex_view.setEnabled(not checked)
        self.timestamps.setEnabled(not checked)
        self.data_tabs.setTabText(0, "终端" if checked else "收发")
        self.data_tabs.setCurrentIndex(0)
        if checked:
            self.terminal.dirty = True
            self.terminal.render()
            self.terminal.setFocus()
        else:
            self.console.setFocus()
            QTimer.singleShot(0, lambda: self.monitor_splitter.setSizes(self.panel_sizes))

    def encoding_changed(self):
        self.reload_view()
        self.parser_changed()

    def parser_changed(self):
        mode = ("off", "auto", "json", "xml")[self.parse_mode.currentIndex()]
        if self.channel_id:
            self.engine.configure_parser(self.channel_id, mode, self.encoding.currentText())
        if mode != "off" and self.data_tabs.indexOf(self.structured_panel) < 0:
            self.add_instrument("structured", activate=False)
        self.filter_structured()

    def filter_structured(self):
        format_filter = {2: "json", 3: "xml"}.get(self.parse_mode.currentIndex())
        search = self.filter.text().casefold()
        for index in range(self.parsed_list.topLevelItemCount()):
            item = self.parsed_list.topLevelItem(index)
            record = item.data(0, Qt.ItemDataRole.UserRole)
            item.setHidden(bool((format_filter and record["format"] != format_filter) or (search and search not in record["pretty"].casefold())))

    def show_parsed(self, current, previous=None):
        record = current.data(0, Qt.ItemDataRole.UserRole) if current else None
        self.parsed_detail.setPlainText(record["pretty"] if record else "")

    def update_structured(self):
        result = self.engine.structured(self.channel_id, cursors=self.parsed_cursors)
        rows = []
        for peer, batch in result["sources"].items():
            self.parsed_cursors[peer] = batch["next_cursor"]
            rows.extend(batch["records"])
        for record in sorted(rows, key=lambda item: item["event_seq"]):
            item = QTreeWidgetItem([record["time"][11:23], record["format"].upper(), record["peer"] or "本连接", record["text"].replace("\n", " ")[:160]])
            item.setData(0, Qt.ItemDataRole.UserRole, record)
            self.parsed_list.addTopLevelItem(item)
            while self.parsed_list.topLevelItemCount() > 200:
                self.parsed_list.takeTopLevelItem(0)
            self.filter_structured()
            if self.follow.isChecked() and not item.isHidden():
                self.parsed_list.setCurrentItem(item)
                self.parsed_list.scrollToItem(item)

    def reload_view(self, *args):
        if not hasattr(self, "console"):
            return
        self.cursor = self.view_start
        self.console.clear()
        self.terminal.reset(self.encoding.currentText() if hasattr(self, "encoding") else "utf-8")

    def clear_view(self):
        self.console.clear()
        self.terminal.reset(self.encoding.currentText())
        self.cursor = self.engine.journal.sequence
        self.view_start = self.cursor
        self.parsed_list.clear()
        self.parsed_detail.clear()
        if self.channel_id:
            self.parsed_cursors = {peer: batch["next_cursor"] for peer, batch in self.engine.structured(self.channel_id, limit=500)["sources"].items()}

    def send_args(self):
        if not self.channel_id:
            raise ValueError("请先建立连接")
        return {
            "channel_id": self.channel_id, "payload": self.payload.toPlainText(),
            "hex_mode": self.send_hex.isChecked(), "encoding": self.encoding.currentText(),
            "newline": ["none", "lf", "cr", "crlf"][self.newline.currentIndex()], "peer": self.peer.currentText().strip(),
        }

    def submit(self, function, *args, **kwargs):
        if len(self.pending) >= 32:
            raise ValueError("发送队列已满")
        future = self.engine.executor.submit(function, *args, **kwargs)
        self.pending.append(future)
        return future

    def error(self, message):
        self.window.show_error(message)

    def send_current(self):
        try:
            self.submit(self.engine.send, **self.send_args())
        except Exception as exc:
            self.error(str(exc))

    def send_terminal(self, data):
        try:
            if not self.channel_id:
                raise ValueError("请先建立连接")
            self.submit(self.engine.send, self.channel_id, data.hex(), hex_mode=True, peer=self.peer.currentText().strip(), source="terminal")
        except Exception as exc:
            self.error(str(exc))

    def toggle_repeat(self, checked):
        if checked:
            try:
                from .data import encode_payload
                self.repeat_args = self.send_args()
                encode_payload(self.repeat_args["payload"], self.repeat_args["encoding"], self.repeat_args["hex_mode"], self.repeat_args["newline"])
                self.repeat_timer.start(self.interval.value())
                self.repeat_send()
            except Exception as exc:
                self.repeat.setChecked(False)
                self.error(str(exc))
        else:
            self.repeat_timer.stop()

    def repeat_send(self):
        if self.repeat_job and not self.repeat_job.done():
            return
        try:
            self.repeat_job = self.submit(self.engine.send, **self.repeat_args)
        except Exception as exc:
            self.repeat.setChecked(False)
            self.error(str(exc))

    def tick(self):
        for future in list(self.pending):
            if future.done():
                self.pending.remove(future)
                try:
                    future.result()
                except Exception as exc:
                    self.repeat.setChecked(False)
                    self.error(str(exc))
        if not self.channel_id:
            self.send_button.setEnabled(False)
            return
        try:
            info = self.engine.get(self.channel_id).describe()
        except ValueError:
            return
        self.channel_title.setText(info["name"])
        self.counters.setText(f'{STATES[info["state"]]}    接收 {info["rx_bytes"]:,} B    发送 {info["tx_bytes"]:,} B')
        self.peer.setVisible(info["config"]["kind"] in ("tcp-server", "udp"))
        ready = info["state"] in ("connected", "listening")
        self.send_button.setEnabled(ready)
        if not ready:
            self.repeat.setChecked(False)
        self.terminal.enabled_input = ready and self.terminal_mode.isChecked()
        if info["config"]["kind"] == "tcp-server":
            peers = [self.peer.itemText(i) for i in range(self.peer.count())]
            if peers != info["peers"]:
                selected = self.peer.currentText()
                self.peer.clear()
                self.peer.addItems(info["peers"])
                if selected in info["peers"]:
                    self.peer.setCurrentText(selected)
        if not self.isVisible():
            return
        if not self.pause.isChecked():
            result = self.engine.journal.read(self.cursor, 500, self.channel_id)
            self.cursor = result["next_cursor"]
            if result["gap"]:
                self.console.append("历史缓冲区已覆盖部分旧数据。")
                self.terminal.reset(self.encoding.currentText())
                self.console.reset_streams()
            scroll = self.console.verticalScrollBar()
            old_scroll = scroll.value()
            for event in result["events"]:
                if event.get("stream_reset"):
                    self.terminal.reset_stream(self.encoding.currentText())
                if event["direction"] == "rx":
                    self.terminal.feed(bytes.fromhex(event["hex"]))
                self.console.feed_event(event, self.encoding.currentText(), self.hex_view.isChecked(), self.filter.text().casefold())
            scroll.setValue(scroll.maximum() if self.follow.isChecked() else old_scroll)
            if self.terminal_mode.isChecked():
                self.terminal.render()
            if self.parse_mode.currentIndex():
                self.update_structured()
        current = self.data_tabs.currentWidget()
        if current in self.instruments:
            current.tick()


    def export_log(self):
        if not self.channel_id:
            return
        path, _ = QFileDialog.getSaveFileName(self, "导出日志", "serial-log.jsonl", "JSON Lines (*.jsonl)")
        if path:
            try:
                records = self.engine.journal.snapshot(self.channel_id)
                Path(path).write_text("\n".join(json.dumps(record, ensure_ascii=False) for record in records) + "\n", encoding="utf-8")
            except OSError as exc:
                self.error(str(exc))

    def refresh_templates(self):
        self.template_box.blockSignals(True)
        self.template_box.clear()
        self.template_box.addItem("报文模板")
        self.template_box.addItems([item["name"] for item in self.window.templates])
        self.template_box.blockSignals(False)

    def apply_template(self, index):
        if index > 0:
            item = self.window.templates[index - 1]
            self.payload.setPlainText(item["payload"])
            self.send_hex.setChecked(item.get("hex_mode", False))
            self.encoding.setCurrentText(item.get("encoding", "utf-8"))
            self.newline.setCurrentIndex(item.get("newline", 0))

    def template_menu(self):
        self.window.activate_pane(self)
        menu = QMenu(self)
        menu.addAction("保存当前报文", self.window.save_template)
        menu.addAction("导入模板", self.window.import_templates)
        menu.addAction("导出模板", self.window.export_templates)
        menu.exec(self.template_box.mapToGlobal(self.template_box.rect().bottomLeft()))

    def dispose(self):
        self.repeat.setChecked(False)
        self.repeat_timer.stop()
        self.terminal.caret_timer.stop()


class ConnectionWorkspace(QWidget):
    activated = Signal(object)
    close_requested = Signal(object)
    menu_requested = Signal(object, object)
    new_requested = Signal()

    def __init__(self):
        super().__init__()
        layout = QVBoxLayout(self)
        layout.setContentsMargins(0, 0, 0, 0)
        layout.setSpacing(0)
        header = QHBoxLayout()
        header.setSpacing(4)
        self.bar = QTabBar()
        self.bar.setExpanding(False)
        self.bar.setMovable(True)
        self.bar.setTabsClosable(True)
        self.bar.setUsesScrollButtons(True)
        self.bar.setElideMode(Qt.TextElideMode.ElideRight)
        self.bar.setContextMenuPolicy(Qt.ContextMenuPolicy.CustomContextMenu)
        self.bar.customContextMenuRequested.connect(self.tab_menu)
        self.bar.currentChanged.connect(self.select_tab)
        self.bar.tabCloseRequested.connect(lambda index: self.close_requested.emit(self.widget(index)))
        self.bar.tabMoved.connect(self.tab_moved)
        self.plus = IconButton("plus", "新建连接")
        self.plus.clicked.connect(self.new_requested)
        header.addWidget(self.bar)
        header.addWidget(self.plus)
        header.addStretch(1)
        layout.addLayout(header)
        self.stack = QStackedWidget()
        layout.addWidget(self.stack, 1)

    def tab_menu(self, point):
        index = self.bar.tabAt(point)
        if index >= 0:
            self.menu_requested.emit(self.widget(index), self.bar.mapToGlobal(point))

    def widget(self, index):
        return self.bar.tabData(index) if index >= 0 else None

    def indexOf(self, pane):
        return next((i for i in range(self.bar.count()) if self.widget(i) is pane), -1)

    def count(self):
        return self.bar.count()

    def select_tab(self, index):
        pane = self.widget(index)
        if pane is not None:
            self.stack.setCurrentWidget(pane)
            self.activated.emit(pane)

    def tab_moved(self, source, target):
        # Tab data travels with the tab; stack order never determines identity.
        self.select_tab(self.bar.currentIndex())

    def add_pane(self, pane, title):
        self.stack.addWidget(pane)
        self.bar.blockSignals(True)
        index = self.bar.addTab(title)
        self.bar.setTabData(index, pane)
        close = CloseTabButton(self.bar)
        close.clicked.connect(lambda: self.close_requested.emit(pane))
        self.bar.setTabButton(index, QTabBar.ButtonPosition.RightSide, close)
        self.bar.blockSignals(False)

    def activate(self, pane):
        index = self.indexOf(pane)
        if index >= 0:
            self.bar.setCurrentIndex(index)
            self.stack.setCurrentWidget(pane)

    def update_pane(self, pane, title, tooltip, icon):
        index = self.indexOf(pane)
        self.bar.setTabText(index, title)
        self.bar.setTabToolTip(index, tooltip)
        self.bar.setTabIcon(index, icon)

    def remove_pane(self, pane):
        index = self.indexOf(pane)
        if index >= 0:
            self.bar.removeTab(index)
            self.stack.removeWidget(pane)
