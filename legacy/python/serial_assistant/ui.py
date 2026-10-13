# SPDX-License-Identifier: Apache-2.0

"""Windows serial/network workbench with independent connection views."""

from dataclasses import asdict
import json
from pathlib import Path

from PySide6.QtCore import Qt, QTimer
from PySide6.QtGui import QColor, QFont, QPalette, QIcon, QPixmap, QPainter
from PySide6.QtWidgets import (
    QApplication, QDialog, QFileDialog, QFontDialog, QFormLayout, QFrame,
    QInputDialog, QLineEdit,
    QMainWindow, QMenu, QScrollArea, QStackedWidget, QTabWidget,
    QTextBrowser, QVBoxLayout, QWidget, QToolButton,
)

from . import __version__
from .engine import ConnectionConfig
from .mcp_panel import MCPPanel
from .runtime import runtime_identity
from .theme import PALETTES, stylesheet
from .views import MonitorView, TerminalView
from .widgets import CheckBox as QCheckBox, ComboBox as QComboBox, ResizableSplitter, SpinBox as QSpinBox, ToggleSwitch, IconButton, button, combo, label, row
from .workspace import ConnectionPane, ConnectionWorkspace, STATES


WAVE_HELP = """
<h2>串口波形的数据格式</h2>
<p>每个采样发送一行文本，以 LF（\n）或 CRLF（\r\n）结束。串口、TCP、UDP 都支持。</p>
<h3>CSV：按顺序生成 CH1、CH2…</h3>
<pre>1.2,3.4,5.6\n</pre>
<h3>命名数值：曲线使用变量名称</h3>
<pre>temperature=25.6,voltage=3.3\n</pre>
<h3>JSON 数值对象或数组</h3>
<pre>{"temperature":25.6,"voltage":3.3}\n
[1.2,3.4,5.6]\n</pre>
<h3>单片机 C 代码</h3>
<pre>printf("temperature=%.2f,voltage=%.3f\\n", temperature, voltage);</pre>
<p>每行最多 16 个有限数值。变量名保持一致，同一行就是同一采样时刻。
普通日志、非数值字段和超长行会被忽略；每个来源保留最近 5000 个采样。</p>
<p>横轴为采样序号，纵轴为报文数值；这是数值报文绘图，不测量 UART 电平时序。
通过“控件 → 图表 → 数值波形”添加波形标签页，滚轮缩放、拖动平移。</p>
"""


class MainWindow(QMainWindow):
    def __init__(self, engine, mcp, settings_path):
        super().__init__()
        self.engine = engine
        self.mcp = mcp
        self.settings_path = Path(settings_path)
        self.selected_id = None
        self.pending = []
        self.templates = []
        self.panes = {}
        self.disconnect_jobs = {}
        self.removing = set()
        self.view_settings = {}
        self.default_appearance = {}
        self.theme_name = "light"
        self.display_font = QFont("Cascadia Mono", 11)
        self.runtime = runtime_identity()
        self._activating = False
        self._loading = True
        self._saved_form = ConnectionConfig()
        self.setWindowTitle(f'Serial Workbench {__version__} · {self.runtime["username"]}')
        self.resize(1500, 920)
        self.setMinimumSize(1100, 760)
        self.setFont(QFont("Microsoft YaHei UI", 10))
        self.build()
        self.apply_theme()
        self.load_settings()
        self._loading = False
        self.timer = QTimer(self)
        self.timer.timeout.connect(self.tick)
        self.timer.start(100)
        QApplication.instance().focusChanged.connect(self.focus_changed)

    def build(self):
        root = QWidget()
        outer = QVBoxLayout(root)
        outer.setContentsMargins(12, 10, 12, 6)
        outer.setSpacing(6)
        self.runtime_notice = label("", "subtitle")
        self.runtime_notice.setWordWrap(True)
        self.runtime_notice.hide()
        outer.addWidget(self.runtime_notice)
        splitter = ResizableSplitter()
        splitter.addWidget(self.build_sidebar())
        splitter.addWidget(self.build_workspace())
        splitter.setStretchFactor(1, 1)
        splitter.setSizes([260, 1210])
        outer.addWidget(splitter, 1)
        self.setCentralWidget(root)
        self.build_settings()

    def build_settings(self):
        self.settings_dialog = QDialog(self)
        self.settings_dialog.setWindowTitle("设置")
        self.settings_dialog.resize(1000, 720)
        body = QVBoxLayout(self.settings_dialog)
        self.settings_tabs = QTabWidget()
        appearance = QWidget()
        form = QFormLayout(appearance)
        form.setContentsMargins(24, 24, 24, 24)
        form.setSpacing(16)
        self.theme_selector = combo(["浅色", "深色"])
        self.theme_selector.currentIndexChanged.connect(self.apply_theme)
        self.font_button = button("选择字体…", self.choose_font)
        self.font_size = QSpinBox()
        self.font_size.setRange(8, 32)
        self.font_size.setValue(11)
        self.font_size.valueChanged.connect(self.change_font_size)
        form.addRow("主题", self.theme_selector)
        form.addRow("数据字体", self.font_button)
        form.addRow("数据字号", self.font_size)
        form.addRow(label("收发、终端与发送内容共用字体；Ctrl + 滚轮调整字号", "subtitle"))
        self.settings_tabs.addTab(appearance, "外观")
        mcp_page = QWidget()
        mcp_body = QVBoxLayout(mcp_page)
        self.mcp_badge = label("MCP", "badge")
        mcp_body.addWidget(self.mcp_badge)
        self.mcp_panel = MCPPanel(self)
        mcp_body.addWidget(self.mcp_panel, 1)
        self.settings_tabs.addTab(mcp_page, "AI / MCP")
        body.addWidget(self.settings_tabs, 1)
        body.addWidget(button("完成", self.settings_dialog.hide))

    def open_settings(self):
        self.settings_dialog.show()
        self.settings_dialog.raise_()
        self.settings_dialog.activateWindow()
        self.color_controls()

    def build_workspace(self):
        self.workspace = ConnectionWorkspace()
        self.draft_pane = ConnectionPane(self)
        self.draft_pane.draft_config = ConnectionConfig()
        self.drafts = [self.draft_pane]
        self.current_pane = self.draft_pane
        self.workspace.add_pane(self.draft_pane, "新连接")
        self.workspace.activate(self.draft_pane)
        self.workspace.activated.connect(self.activate_pane)
        self.workspace.close_requested.connect(self.close_tab)
        self.workspace.menu_requested.connect(self.pane_menu)
        self.workspace.new_requested.connect(self.new_connection)
        return self.workspace

    def all_panes(self):
        return [*self.drafts, *self.panes.values()]

    def apply_theme(self, *args):
        self.theme_name = "dark" if self.theme_selector.currentIndex() == 1 else "light"
        colors = PALETTES[self.theme_name]
        self.setStyleSheet(stylesheet(colors))
        palette = QPalette(self.palette())
        for role, name in {
            QPalette.ColorRole.Window: "base", QPalette.ColorRole.WindowText: "text",
            QPalette.ColorRole.Base: "input", QPalette.ColorRole.Text: "text",
            QPalette.ColorRole.Button: "badge", QPalette.ColorRole.ButtonText: "text",
            QPalette.ColorRole.Highlight: "accent", QPalette.ColorRole.HighlightedText: "button_text",
        }.items():
            palette.setColor(role, QColor(colors[name]))
        palette.setColor(QPalette.ColorRole.Mid, QColor(colors["muted"]))
        self.setPalette(palette)
        self.color_controls()
        for pane in self.all_panes():
            pane.apply_appearance(self.display_font, self.theme_name, replay=True)

    def color_controls(self):
        colors = PALETTES[self.theme_name]
        for control in self.findChildren(ToggleSwitch):
            control.set_colors(colors)
        for control in self.findChildren(QComboBox) + self.findChildren(QSpinBox):
            control.arrow_color = colors["text"]
            control.update()
        for control in self.findChildren(QCheckBox):
            control.indicator_color = colors["muted"]
            control.accent_color = colors["accent"]
            control.fill_color = colors["input"]
            control.update()

    def choose_font(self):
        dialog = QFontDialog(self.display_font, self)
        if dialog.exec():
            self.set_display_font(dialog.selectedFont())

    def set_display_font(self, font):
        self.display_font = QFont(font)
        self.display_font.setPointSize(max(8, min(32, font.pointSize())))
        self.font_size.blockSignals(True)
        self.font_size.setValue(self.display_font.pointSize())
        self.font_size.blockSignals(False)
        self.font_button.setToolTip(self.display_font.family())
        for pane in self.all_panes():
            pane.apply_appearance(self.display_font, self.theme_name)

    def change_font_size(self, size):
        font = QFont(self.display_font)
        font.setPointSize(size)
        self.set_display_font(font)

    @staticmethod
    def view_key(config):
        if config.kind == "serial":
            return "serial:" + config.port.strip().removeprefix("\\\\.\\").casefold()
        return f"{config.kind}:{config.host}:{config.network_port}:{config.bind_host}:{config.bind_port}"

    def load_form(self, cfg):
        for key, widget in {"port": self.port, "baudrate": self.baud, "bytesize": self.bits, "parity": self.parity, "stopbits": self.stopbits, "flow": self.flow}.items():
            widget.setCurrentText(str(getattr(cfg, key)))
        self.name.setText(cfg.name)
        self.host.setText(cfg.host)
        self.bind_host.setText(cfg.bind_host)
        self.net_port.setValue(cfg.network_port)
        self.bind_port.setValue(cfg.bind_port)
        self.dtr.setChecked(cfg.dtr)
        self.rts.setChecked(cfg.rts)
        if cfg.kind != "demo":
            self.protocol.setCurrentIndex(["serial", "tcp-client", "tcp-server", "udp"].index(cfg.kind))

    def activate_pane(self, pane):
        if pane is None or self._activating:
            return
        self._activating = True
        try:
            changed = self.current_pane is not pane
            if changed and self.current_pane in self.drafts:
                try:
                    self.current_pane.draft_config = self.config()
                except ValueError:
                    pass
            self.current_pane = pane
            self.selected_id = pane.channel_id
            self.workspace.activate(pane)
            if changed:
                cfg = self.engine.get(self.selected_id).config if self.selected_id else pane.draft_config
                self.load_form(cfg)
            self.update_connection_controls()
        finally:
            self._activating = False

    def focus_changed(self, previous, current):
        widget = current
        while widget is not None:
            if isinstance(widget, ConnectionPane):
                if widget in self.all_panes():
                    self.activate_pane(widget)
                return
            widget = widget.parentWidget()

    def new_connection(self):
        pane = ConnectionPane(self)
        pane.draft_config = ConnectionConfig()
        pane.restore({key: self.default_appearance[key] for key in ("timestamps", "encoding") if key in self.default_appearance})
        pane.apply_appearance(self.display_font, self.theme_name)
        pane.refresh_templates()
        self.drafts.append(pane)
        self.workspace.add_pane(pane, "新连接")
        self.activate_pane(pane)
        self.load_form(pane.draft_config)
        self.color_controls()
        return pane

    def connect(self):
        try:
            cfg = self.config()
            if cfg.kind == "serial":
                existing = next((item for item in self.engine.list_connections() if self.view_key(ConnectionConfig(**item["config"])) == self.view_key(cfg)), None)
                if existing and existing["state"] in ("connecting", "connected", "reconnecting"):
                    self.sync_connections(existing["id"])
                    return
            info = self.engine.connect(cfg)
            self.sync_connections(info["id"])
        except Exception as exc:
            self.show_error(str(exc))

    def same_endpoint(self, channel):
        try:
            return channel is not None and self.view_key(self.config()) == self.view_key(channel.config)
        except ValueError:
            return False

    def connection_action(self):
        channel = self.engine.get(self.selected_id) if self.selected_id else None
        if self.same_endpoint(channel) and channel.state in ("connecting", "connected", "listening", "reconnecting"):
            self.disconnect()
        elif self.same_endpoint(channel) and channel.config.kind != "serial":
            self.reconnect()
        else:
            self.connect()

    def update_connection_controls(self):
        for channel_id, future in list(self.disconnect_jobs.items()):
            if future.done():
                self.disconnect_jobs.pop(channel_id)
        try:
            channel = self.engine.get(self.selected_id) if self.selected_id in self.panes else None
        except ValueError:
            channel = None
        state = channel.state if channel else "closed"
        busy = bool(channel and channel.id in self.disconnect_jobs)
        text, role = "建立连接", "idle"
        if busy:
            text, role = "正在断开…", "busy"
        elif self.same_endpoint(channel):
            if state == "connecting":
                text, role = "连接中… · 取消", "busy"
            elif state == "reconnecting":
                text, role = "等待恢复… · 断开", "busy"
            elif state in ("connected", "listening"):
                text, role = "停止监听" if state == "listening" else "断开连接", "active"
            else:
                text = "重新连接"
        elif channel and state in ("connecting", "connected", "listening", "reconnecting"):
            text = "建立新连接"
        self.connect_button.setText(text)
        self.connect_button.setEnabled(not busy)
        if self.connect_button.property("connectionState") != role:
            self.connect_button.setProperty("connectionState", role)
            self.connect_button.style().unpolish(self.connect_button)
            self.connect_button.style().polish(self.connect_button)

    def submit(self, function, *args, **kwargs):
        future = self.engine.executor.submit(function, *args, **kwargs)
        self.pending.append(future)
        return future

    def disconnect_id(self, channel_id):
        pane = self.panes.get(channel_id)
        if pane:
            pane.repeat.setChecked(False)
        if channel_id not in self.disconnect_jobs:
            self.disconnect_jobs[channel_id] = self.submit(self.engine.disconnect, channel_id)
            self.update_connection_controls()

    def disconnect(self):
        if self.selected_id:
            self.disconnect_id(self.selected_id)

    def reconnect_id(self, channel_id):
        try:
            self.engine.reconnect(channel_id)
            self.sync_connections(channel_id)
        except Exception as exc:
            self.show_error(str(exc))

    def reconnect(self):
        if self.selected_id:
            self.reconnect_id(self.selected_id)

    def remove_id(self, channel_id):
        if channel_id in self.removing:
            return
        pane = self.panes.get(channel_id)
        if pane:
            try:
                self.view_settings[self.view_key(self.engine.get(channel_id).config)] = pane.appearance()
            except ValueError:
                pass
            pane.dispose()
        previous = self.disconnect_jobs.get(channel_id)
        self.removing.add(channel_id)

        def finish_removal():
            if previous:
                previous.result()
            self.engine.remove(channel_id)

        self.disconnect_jobs[channel_id] = self.submit(finish_removal)

    def remove(self):
        if self.selected_id:
            self.remove_id(self.selected_id)

    def close_tab(self, pane):
        if pane.channel_id:
            self.remove_id(pane.channel_id)
        elif pane in self.drafts:
            self.drafts.remove(pane)
            self.workspace.remove_pane(pane)
            pane.dispose()
            pane.deleteLater()
            if self.workspace.count() == 0:
                self.new_connection()

    def sync_connections(self, select_id=None):
        infos = self.engine.list_connections()
        ids = {item["id"] for item in infos}
        for old_id in list(self.panes):
            if old_id not in ids:
                pane = self.panes.pop(old_id)
                self.removing.discard(old_id)
                pane.dispose()
                self.workspace.remove_pane(pane)
                pane.deleteLater()
        for info in infos:
            cid = info["id"]
            if cid not in self.panes:
                cfg = ConnectionConfig(**info["config"])
                if cid == select_id and self.current_pane in self.drafts:
                    pane = self.current_pane
                    self.drafts.remove(pane)
                    pane.channel_id = cid
                    self.selected_id = cid
                    self.load_form(cfg)
                    saved_view = self.view_settings.get(self.view_key(cfg))
                    if saved_view:
                        pane.restore(saved_view)
                    if pane.parse_mode.currentIndex():
                        pane.parser_changed()
                else:
                    pane = ConnectionPane(self, cid)
                    pane.restore(self.view_settings.get(self.view_key(cfg), self.default_appearance))
                    self.workspace.add_pane(pane, info["name"])
                self.panes[cid] = pane
                pane.apply_appearance(self.display_font, self.theme_name)
                pane.refresh_templates()
                self.color_controls()
            pane = self.panes[cid]
            tooltip = f'{info["name"]} · {STATES[info["state"]]}\n{info["error"] or cid}'
            self.workspace.update_pane(pane, info["name"], tooltip, self.state_icon(info["state"]))
        if select_id in self.panes:
            self.activate_pane(self.panes[select_id])
        if self.workspace.count() == 0:
            self.new_connection()
        self.update_connection_controls()

    def state_icon(self, state):
        colors = PALETTES[self.theme_name]
        key = "rx" if state in ("connected", "listening") else "danger" if state == "error" else "status" if state in ("connecting", "reconnecting") else "muted"
        pixmap = QPixmap(16, 16)
        pixmap.fill(Qt.GlobalColor.transparent)
        painter = QPainter(pixmap)
        painter.setRenderHint(QPainter.RenderHint.Antialiasing)
        painter.setPen(Qt.PenStyle.NoPen)
        painter.setBrush(QColor(colors[key]))
        painter.drawEllipse(4, 4, 8, 8)
        painter.end()
        return QIcon(pixmap)


    def create_connection_menu(self, channel_id):
        info = self.engine.get(channel_id).describe()
        pane = self.panes[channel_id]
        menu = QMenu(self)
        active = info["state"] in ("connecting", "connected", "listening", "reconnecting")
        menu.addAction("断开", lambda: self.disconnect_id(channel_id)).setEnabled(active)
        menu.addAction("重新连接", lambda: self.reconnect_id(channel_id)).setEnabled(not active)
        menu.addAction("重命名", lambda: self.rename_connection(channel_id))
        menu.addSeparator()
        menu.addAction("清屏", pane.clear_view)
        menu.addAction("导出日志", pane.export_log)
        menu.addSeparator()
        menu.addAction("关闭并移除", lambda: self.remove_id(channel_id))
        return menu


    def pane_menu(self, pane, point):
        if pane.channel_id:
            self.create_connection_menu(pane.channel_id).exec(point)


    def rename_connection(self, channel_id):
        channel = self.engine.get(channel_id)
        name, accepted = QInputDialog.getText(self, "重命名", "连接名称", text=channel.name)
        if accepted and name.strip():
            self.engine.rename(channel_id, name)
            if self.selected_id == channel_id:
                self.name.setText(name.strip())
            self.sync_connections()

    def start_demo(self):
        # Kept solely for --demo and release smoke checks; no demo control in the UI.
        info = self.engine.connect(ConnectionConfig(kind="demo", name="模拟信号"))
        self.sync_connections(info["id"])
        self.current_pane.add_instrument("waveform")

    def show_wave_help(self):
        dialog = QDialog(self)
        dialog.setWindowTitle("波形数据格式")
        dialog.resize(680, 600)
        layout = QVBoxLayout(dialog)
        browser = QTextBrowser()
        browser.setHtml(WAVE_HELP)
        layout.addWidget(browser)
        layout.addWidget(button("关闭", dialog.accept))
        dialog.setAttribute(Qt.WidgetAttribute.WA_DeleteOnClose)
        dialog.show()

    def tick(self):
        notice = self.mcp.error
        if self.runtime["sandbox_account"]:
            notice = "当前窗口由受限沙箱账户启动；实体串口请从资源管理器双击程序打开。" + ("\n" + notice if notice else "")
        self.runtime_notice.setText(notice)
        self.runtime_notice.setVisible(bool(notice))
        for future in list(self.pending):
            if future.done():
                self.pending.remove(future)
                try:
                    future.result()
                except Exception as exc:
                    self.show_error(str(exc))
        self.sync_connections()
        self.mcp_badge.setText("MCP 已就绪" if self.mcp.running else "MCP 离线")
        self.mcp_badge.setToolTip(f'{self.runtime["username"]} · PID {self.runtime["pid"]} · {self.mcp.url}')
        for pane in self.all_panes():
            pane.tick()
        self.mcp_panel.tick()

    def refresh_templates(self):
        for pane in self.all_panes():
            pane.refresh_templates()

    def load_settings(self):
        if not self.settings_path.exists():
            return
        try:
            saved = json.loads(self.settings_path.read_text(encoding="utf-8"))
            self.templates = saved.get("templates", [])
            self.validate_templates(self.templates)
            self.refresh_templates()
            self._saved_form = ConnectionConfig(**saved.get("connection", {}))
            if self._saved_form.port == "loop://":
                self._saved_form.port = "NULL"
            self.draft_pane.draft_config = self._saved_form
            self.load_form(self._saved_form)
            self.view_settings = saved.get("views", {})
            appearance = saved.get("appearance", {})
            self.default_appearance = appearance
            self.draft_pane.restore(appearance)
            self.theme_selector.setCurrentIndex(1 if appearance.get("theme") == "dark" else 0)
            self.set_display_font(QFont(appearance.get("font_family", "Cascadia Mono"), int(appearance.get("font_size", 11))))
            self.mcp_panel.ai_control.setChecked(bool(saved.get("allow_ai_control", False)))
            self.engine.allow_ai_control = self.mcp_panel.ai_control.isChecked()
        except (OSError, ValueError, TypeError, AttributeError) as exc:
            self.statusBar().showMessage(f"配置未加载：{exc}")

    def save_settings(self):
        try:
            for cid, pane in self.panes.items():
                if cid in self.engine.channels:
                    self.view_settings[self.view_key(self.engine.get(cid).config)] = pane.appearance()
            appearance = self.current_pane.appearance()
            appearance.update({"theme": self.theme_name, "font_family": self.display_font.family(), "font_size": self.display_font.pointSize()})
            try:
                self._saved_form = self.config()
            except ValueError:
                pass
            saved = {"connection": asdict(self._saved_form), "templates": self.templates, "appearance": appearance, "views": self.view_settings, "allow_ai_control": self.mcp_panel.ai_control.isChecked()}
            self.settings_path.parent.mkdir(parents=True, exist_ok=True)
            temporary = self.settings_path.with_suffix(".tmp")
            temporary.write_text(json.dumps(saved, ensure_ascii=False, indent=2), encoding="utf-8")
            temporary.replace(self.settings_path)
        except (OSError, ValueError) as exc:
            self.statusBar().showMessage(f"配置未保存：{exc}")

    def show_error(self, message):
        self.statusBar().showMessage(f"操作失败：{message}", 10000)

    def closeEvent(self, event):
        self.timer.stop()
        QApplication.instance().focusChanged.disconnect(self.focus_changed)
        self.save_settings()
        for pane in self.all_panes():
            pane.dispose()
        self.mcp.stop()
        self.engine.allow_ai_control = False
        self.engine.close()
        event.accept()

    def build_sidebar(self):
        frame = QFrame()
        frame.setObjectName("sidebar")
        frame.setMinimumWidth(245)
        frame.setMaximumWidth(360)
        body = QVBoxLayout(frame)
        body.setContentsMargins(16, 16, 16, 10)
        body.setSpacing(12)
        body.addWidget(label("Serial Workbench", "brand"))
        scroll = QScrollArea()
        self.parameter_scroll = scroll
        scroll.setWidgetResizable(True)
        scroll.setFrameShape(QFrame.Shape.NoFrame)
        panel = QWidget()
        panel.setObjectName("sidebarContent")
        form_body = QVBoxLayout(panel)
        form_body.setContentsMargins(0, 0, 4, 0)
        form_body.setSpacing(8)
        form_body.addWidget(label("连接参数", "section"))
        self.name = QLineEdit()
        self.name.setPlaceholderText("连接名称（可选）")
        self.protocol = combo(["串口 Serial", "TCP 客户端", "TCP 服务端", "UDP"])
        form_body.addWidget(self.name)
        form_body.addWidget(self.protocol)
        self.config_stack = QStackedWidget()
        serial_panel = QWidget()
        serial_panel.setObjectName("sidebarSection")
        form = QVBoxLayout(serial_panel)
        form.setContentsMargins(0, 4, 0, 0)
        form.setSpacing(8)
        self.port = combo(["NULL"], editable=True)
        form.addWidget(label("端口", "subtitle"))
        form.addWidget(row(self.port, button("刷新", self.refresh_ports)))
        self.baud = combo(["1200", "2400", "4800", "9600", "19200", "38400", "57600", "115200", "230400", "460800", "921600", "1000000", "2000000"], "115200", True)
        form.addWidget(label("波特率", "subtitle"))
        form.addWidget(self.baud)
        self.advanced_button = QToolButton()
        self.advanced_button.setText("更多串口参数")
        self.advanced_button.setCheckable(True)
        self.advanced_button.setToolButtonStyle(Qt.ToolButtonStyle.ToolButtonTextBesideIcon)
        self.advanced_button.setArrowType(Qt.ArrowType.RightArrow)
        form.addWidget(self.advanced_button)
        self.advanced_panel = QWidget()
        self.advanced_panel.setObjectName("sidebarSection")
        advanced = QFormLayout(self.advanced_panel)
        advanced.setContentsMargins(0, 4, 0, 0)
        advanced.setLabelAlignment(Qt.AlignmentFlag.AlignLeft | Qt.AlignmentFlag.AlignVCenter)
        advanced.setFormAlignment(Qt.AlignmentFlag.AlignTop)
        advanced.setFieldGrowthPolicy(QFormLayout.FieldGrowthPolicy.AllNonFixedFieldsGrow)
        advanced.setHorizontalSpacing(10)
        advanced.setVerticalSpacing(8)
        self.bits = combo(["5", "6", "7", "8"], "8")
        self.parity = combo(["N", "E", "O", "M", "S"])
        self.stopbits = combo(["1", "1.5", "2"])
        self.flow = combo(["none", "rtscts", "xonxoff"])
        self.dtr = ToggleSwitch("DTR")
        self.rts = ToggleSwitch("RTS")
        for title, widget in (("数据位", self.bits), ("校验位", self.parity), ("停止位", self.stopbits), ("流控", self.flow)):
            advanced.addRow(title, widget)
        advanced.addRow(self.dtr)
        advanced.addRow(self.rts)
        form.addWidget(self.advanced_panel)
        self.advanced_panel.hide()
        self.advanced_button.toggled.connect(self.advanced_panel.setVisible)
        self.advanced_button.toggled.connect(lambda checked: self.advanced_button.setArrowType(Qt.ArrowType.DownArrow if checked else Qt.ArrowType.RightArrow))
        form.addStretch(1)
        self.config_stack.addWidget(serial_panel)
        net_panel = QWidget()
        net_panel.setObjectName("sidebarSection")
        net = QVBoxLayout(net_panel)
        net.setContentsMargins(0, 4, 0, 0)
        self.host = QLineEdit("127.0.0.1")
        self.net_port = QSpinBox()
        self.net_port.setRange(1, 65535)
        self.net_port.setValue(9000)
        self.bind_host = QLineEdit("127.0.0.1")
        self.bind_port = QSpinBox()
        self.bind_port.setRange(0, 65535)
        self.bind_port.setValue(9001)
        for title, widget in (("远端地址", self.host), ("远端端口", self.net_port), ("本地地址", self.bind_host), ("本地端口", self.bind_port)):
            net.addWidget(label(title, "subtitle"))
            net.addWidget(widget)
        net.addStretch(1)
        self.config_stack.addWidget(net_panel)
        self.protocol.currentIndexChanged.connect(self.protocol_changed)
        form_body.addWidget(self.config_stack)
        self.connect_button = button("建立连接", self.connection_action, True)
        form_body.addWidget(self.connect_button)
        form_body.addStretch(1)
        scroll.setWidget(panel)
        body.addWidget(scroll, 1)
        self.settings_button = IconButton("gear", "设置", frame)
        body.addWidget(row(self.settings_button, label(f"v{__version__}", "subtitle")))
        self.settings_button.clicked.connect(self.open_settings)
        self.refresh_ports()
        return frame


    def protocol_changed(self, index):
        self.config_stack.setCurrentIndex(0 if index == 0 else 1)
        self.host.setEnabled(index in (1, 3))
        self.net_port.setEnabled(index in (1, 3))
        self.bind_host.setEnabled(index in (2, 3))
        self.bind_port.setEnabled(index in (2, 3))


    def config(self):
        return ConnectionConfig(
            kind=["serial", "tcp-client", "tcp-server", "udp", "demo"][self.protocol.currentIndex()],
            name=self.name.text().strip(), port=self.port.currentText().strip(), baudrate=int(self.baud.currentText()),
            bytesize=int(self.bits.currentText()), parity=self.parity.currentText(), stopbits=float(self.stopbits.currentText()),
            flow=self.flow.currentText(), dtr=self.dtr.isChecked(), rts=self.rts.isChecked(),
            host=self.host.text().strip(), network_port=self.net_port.value(), bind_host=self.bind_host.text().strip(), bind_port=self.bind_port.value(),
        )


    def refresh_ports(self):
        previous = self.port.currentText()
        self.port.clear()
        self.port.addItem("NULL")
        for port in self.engine.serial_ports():
            self.port.addItem(port["port"])
            self.port.setItemData(self.port.count() - 1, port["description"], Qt.ItemDataRole.ToolTipRole)
        self.port.setCurrentText(previous or "NULL")


    @staticmethod
    def validate_templates(templates):
        if not isinstance(templates, list) or len(templates) > 500:
            raise ValueError("模板必须为 JSON 数组，最多 500 条")
        for item in templates:
            if not isinstance(item, dict) or not isinstance(item.get("name"), str) or not isinstance(item.get("payload"), str):
                raise ValueError("每个模板需要 name 和 payload 字符串")
            if item.get("encoding", "utf-8") not in ("utf-8", "gbk", "ascii", "latin-1") or item.get("newline", 0) not in range(4):
                raise ValueError("模板编码或换行格式无效")


    def import_templates(self):
        path, _ = QFileDialog.getOpenFileName(self, "导入报文模板", "", "JSON (*.json)")
        if path:
            try:
                templates = json.loads(Path(path).read_text(encoding="utf-8"))
                self.validate_templates(templates)
                self.templates.extend(templates)
                self.refresh_templates()
                self.save_settings()
            except (OSError, ValueError, TypeError) as exc:
                self.show_error(str(exc))


    def export_templates(self):
        path, _ = QFileDialog.getSaveFileName(self, "导出报文模板", "templates.json", "JSON (*.json)")
        if path:
            try:
                Path(path).write_text(json.dumps(self.templates, ensure_ascii=False, indent=2), encoding="utf-8")
            except OSError as exc:
                self.show_error(str(exc))


    def save_template(self):
        name, ok = QInputDialog.getText(self, "保存报文模板", "模板名称")
        if ok and name.strip():
            self.templates.append({"name": name.strip(), "payload": self.current_pane.payload.toPlainText(), "hex_mode": self.current_pane.send_hex.isChecked(), "encoding": self.current_pane.encoding.currentText(), "newline": self.current_pane.newline.currentIndex()})
            self.refresh_templates()
            self.save_settings()

