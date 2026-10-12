"""MCP connection overview, per-client setup and live call log."""

import json
from PySide6.QtCore import Qt
from PySide6.QtGui import QFont
from PySide6.QtWidgets import QApplication, QLineEdit, QPlainTextEdit, QTabWidget, QTreeWidget, QTreeWidgetItem, QVBoxLayout, QWidget

from .client_setup import CLIENTS, ClientSetup
from .widgets import CheckBox, ToggleSwitch, button, combo, label, row
from .workspace import STATES


class MCPPanel(QWidget):
    def __init__(self, window):
        super().__init__()
        self.window = window
        self.engine = window.engine
        self.service = window.mcp
        self.setup = ClientSetup(self.service.url)
        self.jobs = {}
        self.log_items = {}
        self.log_start = 0
        self.connection_items = {}
        layout = QVBoxLayout(self)
        layout.setContentsMargins(12, 10, 12, 10)
        self.address = QLineEdit(self.service.url)
        self.address.setReadOnly(True)
        self.ai_control = ToggleSwitch("允许 AI 发送报文和控制连接")
        self.ai_control.toggled.connect(self.permission_changed)
        layout.addWidget(row(self.address, button("复制地址", lambda: QApplication.clipboard().setText(self.service.url))))
        layout.addWidget(self.ai_control)
        self.status = label("", "subtitle")
        layout.addWidget(self.status)
        tabs = QTabWidget()
        overview = QWidget()
        body = QVBoxLayout(overview)
        self.summary = label("", "section")
        body.addWidget(row(self.summary, button("复制 AI 提示词", self.copy_prompt)))
        self.connection_table = QTreeWidget()
        self.connection_table.setHeaderLabels(["连接", "协议", "状态", "连接 ID", "RX / TX", "对端"])
        self.connection_table.setRootIsDecorated(False)
        self.connection_table.setColumnWidth(0, 180)
        self.connection_table.setColumnWidth(1, 95)
        body.addWidget(self.connection_table)
        tabs.addTab(overview, "全部连接")
        clients = QWidget()
        body = QVBoxLayout(clients)
        self.setup_buttons = []
        self.setup_status = []
        for index, name in enumerate(CLIENTS):
            action = button("一键配置并检查", lambda checked=False, i=index: self.configure(i))
            self.setup_buttons.append(action)
            body.addWidget(row(label(name, "section"), action))
            status = label("", "subtitle")
            status.setWordWrap(True)
            status.hide()
            self.setup_status.append(status)
            body.addWidget(status)
        self.client_selector = combo(list(CLIENTS))
        self.client_selector.currentIndexChanged.connect(self.preview)
        body.addWidget(row(self.client_selector, button("复制配置", self.copy_config)))
        self.client_config = QPlainTextEdit()
        self.client_config.setReadOnly(True)
        self.client_config.setFont(QFont("Consolas", 10))
        body.addWidget(self.client_config, 1)
        self.preview()
        tabs.addTab(clients, "客户端接入")
        audit = QWidget()
        body = QVBoxLayout(audit)
        self.client_filter = combo(["全部客户端"])
        self.client_filter.currentIndexChanged.connect(self.filter_logs)
        self.log_follow = CheckBox("跟随最新")
        self.log_follow.setChecked(True)
        body.addWidget(row(self.client_filter, self.log_follow, button("清屏", self.clear_logs)))
        self.call_log = QTreeWidget()
        self.call_log.setHeaderLabels(["时间", "客户端", "工具", "状态", "耗时 ms", "参数 / 错误"])
        self.call_log.setRootIsDecorated(False)
        for column, width in enumerate((100, 130, 140, 65, 80)):
            self.call_log.setColumnWidth(column, width)
        body.addWidget(self.call_log)
        tabs.addTab(audit, "调用日志")
        layout.addWidget(tabs, 1)

    def permission_changed(self, enabled):
        self.engine.allow_ai_control = enabled
        if hasattr(self.window, "timer"):
            self.window.save_settings()

    def preview(self):
        self.client_config.setPlainText(self.setup.preview(self.client_selector.currentIndex()))

    def copy_config(self):
        QApplication.clipboard().setText(self.client_config.toPlainText())

    def copy_prompt(self):
        QApplication.clipboard().setText(
            "使用 serial-workbench，先调用 get_status 查看全部串口、TCP、UDP 连接及各协议数量。"
            "read_events 不指定 channel_id 可读取所有连接，按 channel_id 区分数据；"
            "逐个读取时分别保存 next_cursor。GUI 当前标签不限制可读取的连接。"
            "解析已开启时可调用 read_structured 获取 JSON/XML，按来源保存返回的游标。"
        )

    def configure(self, index):
        if index in self.jobs:
            return
        self.setup_buttons[index].setEnabled(False)
        self.setup_status[index].show()
        self.setup_status[index].setText("正在配置并检查…")
        self.jobs[index] = self.engine.executor.submit(self.setup.configure_and_check, index)

    def clear_logs(self):
        self.log_start = self.service.audit.sequence
        self.log_items.clear()
        self.call_log.clear()

    def filter_logs(self):
        selected = self.client_filter.currentText()
        for item in self.log_items.values():
            item.setHidden(self.client_filter.currentIndex() > 0 and item.text(1) != selected)

    def tick(self):
        for index, future in list(self.jobs.items()):
            if future.done():
                self.jobs.pop(index)
                self.setup_buttons[index].setEnabled(True)
                try:
                    self.setup_status[index].setText(future.result())
                except Exception as exc:
                    self.setup_status[index].setText(f"配置失败：{exc}")
        self.status.setText(self.service.error or f'{"运行中" if self.service.running else "离线"} · {self.service.calls} 次调用')
        connections = self.engine.list_connections()
        ids = {info["id"] for info in connections}
        for cid in list(self.connection_items):
            if cid not in ids:
                item = self.connection_items.pop(cid)
                self.connection_table.takeTopLevelItem(self.connection_table.indexOfTopLevelItem(item))
        for info in connections:
            cid = info["id"]
            if cid not in self.connection_items:
                self.connection_items[cid] = QTreeWidgetItem()
                self.connection_table.addTopLevelItem(self.connection_items[cid])
            item = self.connection_items[cid]
            values = [info["name"], info["config"]["kind"], STATES[info["state"]], cid, f'{info["rx_bytes"]:,} / {info["tx_bytes"]:,}', ", ".join(info["peers"])]
            for column, value in enumerate(values):
                item.setText(column, value)
        active = [item for item in connections if item["state"] in ("connected", "listening")]
        serial = sum(item["config"]["kind"] == "serial" for item in active)
        tcp = sum(item["config"]["kind"].startswith("tcp-") for item in active)
        udp = sum(item["config"]["kind"] == "udp" for item in active)
        self.summary.setText(f"{len(active)} 个连接 · 串口 {serial} · TCP {tcp} · UDP {udp}")
        if not self.isVisible():
            return
        entries = [entry for entry in self.service.audit.snapshot() if entry["seq"] > self.log_start][-300:]
        ids = {entry["seq"] for entry in entries}
        for seq in list(self.log_items):
            if seq not in ids:
                item = self.log_items.pop(seq)
                self.call_log.takeTopLevelItem(self.call_log.indexOfTopLevelItem(item))
        for entry in entries:
            seq = entry["seq"]
            if seq not in self.log_items:
                item = QTreeWidgetItem()
                self.log_items[seq] = item
                self.call_log.addTopLevelItem(item)
            item = self.log_items[seq]
            summary = entry["error"] or json.dumps(entry["arguments"], ensure_ascii=False)
            columns = [entry["time"][11:23], entry["client"], entry["tool"], {"ok": "成功", "error": "失败", "running": "执行中"}[entry["status"]], str(entry["duration_ms"]), summary]
            for column, value in enumerate(columns):
                item.setText(column, value)
                item.setToolTip(column, value)
            if self.client_filter.findText(entry["client"]) < 0:
                self.client_filter.addItem(entry["client"])
        self.filter_logs()
        if entries and self.log_follow.isChecked():
            self.call_log.scrollToBottom()
