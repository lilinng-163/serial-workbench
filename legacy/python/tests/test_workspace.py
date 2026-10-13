# SPDX-License-Identifier: Apache-2.0

import asyncio
import json
import socket

import pytest

from mcp import ClientSession, types
from mcp.client.streamable_http import streamable_http_client
from PySide6.QtCore import QCoreApplication, QEvent, QPoint, QPointF, Qt
from PySide6.QtGui import QWheelEvent
from PySide6.QtTest import QTest
from PySide6.QtWidgets import QApplication, QDialog, QPushButton, QTextBrowser

from serial_assistant.engine import ConnectionConfig, Engine
from serial_assistant.mcp_server import MCPService
from serial_assistant.ui import MainWindow
from serial_assistant.client_setup import ClientSetup
from test_ui import pump
from test_mcp import result_data, service


def open_loop(window, app):
    window.port.setCurrentText("loop://")
    window.connect()
    pump(app, lambda: window.selected_id and window.engine.get(window.selected_id).state == "connected")
    return window.current_pane


def test_repeated_reconnect_reuses_tab_connection_id_and_raw_history(engine, tmp_path):
    app = QApplication.instance() or QApplication([])
    window = MainWindow(engine, MCPService(engine, 19876), tmp_path / "settings.json")
    window.show()
    try:
        pane = open_loop(window, app)
        cid = pane.channel_id
        pane.payload.setPlainText("previous boot")
        pane.send_current()
        pump(app, lambda: "previous boot" in pane.console.toPlainText())
        before = engine.journal.snapshot(cid)
        for _ in range(3):
            window.disconnect()
            pump(app, lambda: engine.get(cid).state == "closed" and not window.disconnect_jobs)
            window.reconnect()
            pump(app, lambda: engine.get(cid).state == "connected")
            assert window.selected_id == cid
            assert window.current_pane is pane
            assert len(window.panes) == window.workspace.count() == 1
        assert engine.journal.snapshot(cid)[:len(before)] == before
        assert "previous boot" in pane.console.toPlainText()
    finally:
        window.close()


def test_connection_tabs_keep_receive_send_and_waveform_independent(engine, tmp_path):
    app = QApplication.instance() or QApplication([])
    window = MainWindow(engine, MCPService(engine, 19876), tmp_path / "settings.json")
    window.show()
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as remote:
        remote.bind(("127.0.0.1", 0))
        remote.settimeout(2)
        try:
            left = open_loop(window, app)
            wave = left.add_instrument("waveform")
            left.payload.setPlainText("1,2")
            left.send_current()
            pump(app, lambda: len(wave.curves) == 2)
            udp = engine.connect(ConnectionConfig(kind="udp", bind_port=0, network_port=remote.getsockname()[1], name="Network"))
            window.sync_connections(udp["id"])
            right = window.current_pane
            right.payload.setPlainText("right only")
            right.send_current()
            assert remote.recvfrom(1000)[0] == b"right only\r\n"
            engine.get(right.channel_id).receive(b"network RX\n")
            pump(app, lambda: "network RX" in right.console.toPlainText())
            assert "1,2" not in right.console.toPlainText()
            window.activate_pane(left)
            left.data_tabs.setCurrentIndex(0)
            pump(app, lambda: "1,2" in left.console.toPlainText())
            assert "network RX" not in left.console.toPlainText()
            assert left.payload.toPlainText() == "1,2"
            assert right.payload.toPlainText() == "right only"
            assert window.workspace.count() == 2
            assert left.isVisible() and not right.isVisible()
        finally:
            window.close()


def test_context_menu_target_and_tab_close_leave_other_connections_running(engine, tmp_path):
    app = QApplication.instance() or QApplication([])
    window = MainWindow(engine, MCPService(engine, 19876), tmp_path / "settings.json")
    window.show()
    try:
        first = open_loop(window, app)
        second = engine.connect(ConnectionConfig(kind="udp", bind_port=0))
        window.sync_connections(second["id"])
        pump(app, lambda: engine.get(second["id"]).state == "connected")
        menu = window.create_connection_menu(first.channel_id)
        actions = {action.text(): action for action in menu.actions()}
        assert {"断开", "重新连接", "重命名", "清屏", "导出日志", "关闭并移除"} <= set(actions)
        assert actions["断开"].isEnabled() and not actions["重新连接"].isEnabled()
        actions["断开"].trigger()
        pump(app, lambda: engine.get(first.channel_id).state == "closed")
        assert engine.get(second["id"]).state == "connected"
        window.close_tab(first)
        pump(app, lambda: first.channel_id not in window.panes)
        assert len(window.panes) == 1
        assert engine.get(second["id"]).state == "connected"
    finally:
        window.close()


def test_closing_active_connection_tab_selects_surviving_pane_and_saves_settings(engine, tmp_path):
    app = QApplication.instance() or QApplication([])
    path = tmp_path / "settings.json"
    window = MainWindow(engine, MCPService(engine, 19876), path)
    window.show()
    try:
        left = open_loop(window, app)
        other = engine.connect(ConnectionConfig(kind="udp", bind_port=0))
        window.sync_connections(other["id"])
        right = window.current_pane
        pump(app, lambda: engine.get(other["id"]).state == "connected")

        left.timestamps.setChecked(False)
        left.payload.setPlainText("keep this draft")
        window.connect_button.setFocus()
        app.processEvents()
        window.close_tab(right)
        pump(app, lambda: other["id"] not in window.panes)
        QCoreApplication.sendPostedEvents(None, QEvent.Type.DeferredDelete)

        assert window.current_pane is left
        assert window.selected_id == left.channel_id
        assert window.workspace.widget(window.workspace.bar.currentIndex()) is left
        assert engine.get(left.channel_id).state == "connected"
        assert left.payload.toPlainText() == "keep this draft"
        window.save_settings()
        assert json.loads(path.read_text(encoding="utf-8"))["appearance"]["timestamps"] is False
    finally:
        window.close()


def test_restoring_terminal_with_parser_keeps_terminal_visible_and_receives_both(engine, tmp_path):
    app = QApplication.instance() or QApplication([])
    path = tmp_path / "settings.json"
    path.write_text(json.dumps({
        "connection": {"port": "loop://"},
        "appearance": {"terminal_mode": True, "parser": 1},
    }), encoding="utf-8")
    window = MainWindow(engine, MCPService(engine, 19876), path)
    window.show()
    try:
        pane = open_loop(window, app)
        assert pane.terminal_mode.isChecked()
        assert pane.data_tabs.currentIndex() == 0
        assert pane.terminal.isVisible()
        assert not pane.sender_panel.isVisible()
        assert engine.get(pane.channel_id).parser_mode == "auto"

        engine.get(pane.channel_id).receive(b'{"temperature":25.6}\r\n')
        pump(app, lambda: pane.parsed_list.topLevelItemCount() == 1)
        assert '"temperature":25.6' in pane.terminal.toPlainText()
        assert pane.data_tabs.currentIndex() == 0
        pane.data_tabs.setCurrentIndex(1)
        assert pane.parsed_list.isVisible()
        assert "25.6" in pane.parsed_detail.toPlainText()
    finally:
        window.close()


@pytest.mark.parametrize("size", [(1100, 760), (1500, 920)])
def test_receive_sender_handle_drags_and_preserves_ratio(engine, tmp_path, size):
    app = QApplication.instance() or QApplication([])
    path = tmp_path / "settings.json"
    window = MainWindow(engine, MCPService(engine, 19876), path)
    window.resize(*size)
    window.show()
    try:
        pane = open_loop(window, app)
        QTest.qWait(120)
        pane.monitor_splitter.setSizes([400, 260])
        before = pane.monitor_splitter.sizes()
        handle = pane.monitor_splitter.handle(1)
        start = handle.rect().center()
        QTest.mousePress(handle, Qt.MouseButton.LeftButton, pos=start)
        QTest.mouseMove(handle, start + QPoint(0, 48), delay=20)
        QTest.mouseRelease(handle, Qt.MouseButton.LeftButton, pos=start + QPoint(0, 48))
        QTest.qWait(40)
        resized = pane.monitor_splitter.sizes()
        assert resized[0] >= before[0] + 20
        assert resized[1] <= before[1] - 20
        assert pane.console.isVisible() and pane.sender_panel.isVisible()
        wave = pane.add_instrument("waveform")
        assert wave.isVisible() and not pane.monitor_panel.isVisible()
        pane.data_tabs.setCurrentIndex(0)
        restored = pane.monitor_splitter.sizes()
        assert abs(restored[0] / sum(restored) - resized[0] / sum(resized)) < 0.02
        window.save_settings()
        saved = json.loads(path.read_text(encoding="utf-8"))
        key = window.view_key(engine.get(pane.channel_id).config)
        assert saved["views"][key]["sender_sizes"] == restored
        pane.terminal_mode.setChecked(True)
        assert not pane.sender_panel.isVisible()
        pane.terminal_mode.setChecked(False)
        assert pane.sender_panel.isVisible()
    finally:
        window.close()


def test_instrument_pages_and_sender_ratio_restore_after_restart(tmp_path):
    app = QApplication.instance() or QApplication([])
    path = tmp_path / "settings.json"
    engine = Engine()
    first = MainWindow(engine, MCPService(engine, 19876), path)
    first.show()
    try:
        pane = open_loop(first, app)
        pane.add_instrument("waveform").count.setValue(600)
        pane.add_instrument("values")
        pane.data_tabs.setCurrentIndex(0)
        app.processEvents()
        pane.monitor_splitter.setSizes([450, 180])
        sizes = pane.monitor_splitter.sizes()
        ratio = sizes[0] / sum(sizes)
    finally:
        first.close()
    engine = Engine()
    second = MainWindow(engine, MCPService(engine, 19876), path)
    second.show()
    try:
        pane = open_loop(second, app)
        app.processEvents()
        assert [item.kind for item in pane.instruments] == ["waveform", "values"]
        assert pane.instruments[0].count.value() == 600
        assert pane.data_tabs.currentIndex() == 0
        sizes = pane.monitor_splitter.sizes()
        assert abs(sizes[0] / sum(sizes) - ratio) < 0.02
    finally:
        second.close()


def test_existing_waveform_legend_text_repaints_when_theme_changes(engine, tmp_path):
    app = QApplication.instance() or QApplication([])
    window = MainWindow(engine, MCPService(engine, 19876), tmp_path / "settings.json")
    window.show()
    try:
        pane = open_loop(window, app)
        wave = pane.add_instrument("waveform")
        engine.get(pane.channel_id).receive(b'temperature=25.6,voltage=3.3\n')
        pump(app, lambda: len(wave.curves) == 2)
        for theme_index, expected_color in [(1, "#dce7f3"), (0, "#24334a")]:
            window.theme_selector.setCurrentIndex(theme_index)
            for sample, text in wave.plot.getPlotItem().legend.items:
                fragment = text.item.document().firstBlock().begin().fragment()
                assert fragment.charFormat().foreground().color().name() == expected_color
    finally:
        window.close()


def test_parameter_wheel_never_changes_values_and_demo_is_absent(engine, tmp_path):
    app = QApplication.instance() or QApplication([])
    window = MainWindow(engine, MCPService(engine, 19876), tmp_path / "settings.json")
    window.resize(1100, 760)
    window.show()
    try:
        app.processEvents()
        controls = [window.port, window.baud, window.bits, window.parity, window.stopbits, window.flow, window.protocol, window.net_port, window.bind_port]
        for control in controls:
            previous = control.currentText() if hasattr(control, "currentText") else control.value()
            control.setFocus()
            event = QWheelEvent(QPointF(10, 10), QPointF(control.mapToGlobal(QPoint(10, 10))), QPoint(), QPoint(0, -120), Qt.MouseButton.NoButton, Qt.KeyboardModifier.NoModifier, Qt.ScrollPhase.NoScrollPhase, False)
            app.sendEvent(control, event)
            assert (control.currentText() if hasattr(control, "currentText") else control.value()) == previous
        assert window.protocol.count() == 4
        assert all("演示" not in button.text() for button in window.findChildren(QPushButton))
        window.show_wave_help()
        app.processEvents()
        dialog = window.findChildren(QDialog)[-1]
        content = dialog.findChild(QTextBrowser).toPlainText()
        assert "printf" in content and "JSON" in content and "LF" in content
        dialog.close()
    finally:
        window.close()


def test_ai_permission_is_saved_immediately_and_restored_even_with_invalid_form(tmp_path):
    app = QApplication.instance() or QApplication([])
    path = tmp_path / "settings.json"
    engine = Engine()
    window = MainWindow(engine, MCPService(engine, 19876), path)
    window.show()
    window.baud.setCurrentText("invalid draft")
    window.mcp_panel.ai_control.setChecked(True)
    assert json.loads(path.read_text(encoding="utf-8"))["allow_ai_control"] is True
    window.close()
    engine2 = Engine()
    reopened = MainWindow(engine2, MCPService(engine2, 19876), path)
    try:
        assert reopened.mcp_panel.ai_control.isChecked()
        assert engine2.allow_ai_control
        reopened.mcp_panel.ai_control.setChecked(False)
        assert json.loads(path.read_text(encoding="utf-8"))["allow_ai_control"] is False
    finally:
        reopened.close()


def test_structured_tab_filters_formats_keeps_raw_and_replays_no_duplicates(engine, tmp_path):
    app = QApplication.instance() or QApplication([])
    window = MainWindow(engine, MCPService(engine, 19876), tmp_path / "settings.json")
    window.show()
    try:
        pane = open_loop(window, app)
        pane.parse_mode.setCurrentIndex(1)
        engine.get(pane.channel_id).receive(b'log {"a":')
        engine.get(pane.channel_id).receive(b'1}\n<state><x>2</x></state> tail\n')
        pump(app, lambda: pane.parsed_list.topLevelItemCount() == 2)
        assert '{"a":1}' in pane.console.toPlainText()
        assert "<state>" in pane.parsed_detail.toPlainText()
        pane.parse_mode.setCurrentIndex(2)
        assert pane.parsed_list.topLevelItem(1).isHidden()
        engine.get(pane.channel_id).receive(b'<ignored/> {"b":2}')
        pump(app, lambda: pane.parsed_list.topLevelItemCount() == 3)
        pane.tick()
        assert pane.parsed_list.topLevelItemCount() == 3
        pane.clear_view()
        pane.tick()
        assert pane.parsed_list.topLevelItemCount() == 0
        assert b'{"a":' in b"".join(bytes.fromhex(event["hex"]) for event in engine.journal.snapshot(pane.channel_id))
    finally:
        window.close()


def test_mcp_all_channels_counts_client_call_logs_and_structured_reads(engine, service, tmp_path):
    app = QApplication.instance() or QApplication([])
    window = MainWindow(engine, service, tmp_path / "settings.json")
    window.show()
    try:
        pane = open_loop(window, app)
        other = engine.connect(ConnectionConfig(kind="udp", bind_port=0))
        window.sync_connections(other["id"])
        pump(app, lambda: engine.get(other["id"]).state == "connected")
        pane.parse_mode.setCurrentIndex(1)
        engine.get(pane.channel_id).receive(b'{"temperature":25.6}\n')
        engine.get(other["id"]).receive(b"network data\n")

        async def exercise():
            async with streamable_http_client(service.url) as (read, write, _):
                async with ClientSession(read, write, client_info=types.Implementation(name="test-agent-A", version="1")) as client:
                    await client.initialize()
                    status = result_data(await client.call_tool("get_status"))
                    assert status["connected_count"] == 2
                    assert status["counts_by_protocol"]["serial"] == 1
                    assert status["counts_by_protocol"]["udp"] == 1
                    assert {item["id"] for item in status["connections"]} == {pane.channel_id, other["id"]}
                    events = result_data(await client.call_tool("read_events"))["events"]
                    assert {item["channel_id"] for item in events} == {pane.channel_id, other["id"]}
                    parsed = result_data(await client.call_tool("read_structured", {"channel_id": pane.channel_id}))
                    assert parsed["sources"]["default"]["records"][0]["value"]["temperature"] == 25.6
                    denied = await client.call_tool("send_data", {"channel_id": pane.channel_id, "payload": "sensitive example"})
                    assert denied.isError
        asyncio.run(exercise())
        entries = service.audit.snapshot()
        assert all(item["client"] == "test-agent-A" for item in entries)
        denied = next(item for item in entries if item["tool"] == "send_data")
        assert denied["status"] == "error" and denied["error"]
        assert "sensitive example" not in json.dumps(denied)
        assert all(item["duration_ms"] >= 0 for item in entries)
        window.open_settings()
        window.settings_tabs.setCurrentIndex(1)
        window.tick()
        assert window.mcp_panel.call_log.topLevelItemCount() == len(entries)
        assert window.mcp_panel.connection_table.topLevelItemCount() == 2
        assert len(window.mcp_panel.setup_buttons) == 4
    finally:
        window.close()


def test_all_client_setup_buttons_write_isolated_configs_and_check_live_mcp(engine, service, tmp_path, monkeypatch):
    monkeypatch.delenv("CODEX_HOME", raising=False)
    monkeypatch.delenv("XDG_CONFIG_HOME", raising=False)
    app = QApplication.instance() or QApplication([])
    window = MainWindow(engine, service, tmp_path / "settings.json")
    window.show()
    try:
        panel = window.mcp_panel
        panel.setup = ClientSetup(service.url, home=tmp_path / "clients", appdata=tmp_path / "AppData")
        window.open_settings()
        window.settings_tabs.setCurrentIndex(1)
        for action in panel.setup_buttons:
            action.click()
            assert not action.isEnabled()
        pump(app, lambda: not panel.jobs, timeout=20)
        for index, status in enumerate(panel.setup_status):
            assert "检查通过" in status.text(), status.text()
            assert panel.setup.path(index).exists()
            assert panel.setup_buttons[index].isEnabled()
        assert not engine.list_connections()
    finally:
        window.close()
