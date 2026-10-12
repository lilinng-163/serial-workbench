import time
import threading

from PySide6.QtCore import Qt, QPoint, QPointF
from PySide6.QtTest import QTest
from PySide6.QtGui import QFont, QWheelEvent, QTextCursor
from PySide6.QtWidgets import QApplication

from serial_assistant.mcp_server import MCPService
from serial_assistant.engine import Channel
from serial_assistant.ansi import ansi_color
from serial_assistant.ui import MainWindow, MonitorView, TerminalView


def pump(app, condition, timeout=4):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        app.processEvents()
        if condition():
            return
        QTest.qWait(20)
    raise AssertionError("UI condition timed out")


def test_gui_send_monitor_clear_and_mcp_journal(engine, tmp_path):
    app = QApplication.instance() or QApplication([])
    window = MainWindow(engine, MCPService(engine, 19876), tmp_path / "settings.json")
    window.show()
    try:
        window.port.setCurrentText("loop://")
        window.connect()
        pump(app, lambda: window.selected_id and engine.get(window.selected_id).state == "connected")
        window.current_pane.payload.setPlainText("screen and AI")
        QTest.mouseClick(window.current_pane.send_button, Qt.MouseButton.LeftButton)
        pump(app, lambda: "screen and AI" in window.current_pane.console.toPlainText())
        journal_before = engine.journal.snapshot(window.selected_id)
        window.current_pane.clear_view()
        assert window.current_pane.console.toPlainText() == ""
        assert engine.journal.snapshot(window.selected_id) == journal_before
        window.current_pane.payload.setPlainText("periodic")
        window.current_pane.interval.setValue(30)
        window.current_pane.repeat.setChecked(True)
        pump(app, lambda: engine.get(window.selected_id).tx_bytes > 25)
        window.current_pane.repeat.setChecked(False)
        window.open_settings()
        window.settings_tabs.setCurrentIndex(1)
        window.mcp_panel.ai_control.setChecked(True)
        assert engine.allow_ai_control
        window.mcp_panel.client_selector.setCurrentIndex(2)
        assert '"type": "remote"' in window.mcp_panel.client_config.toPlainText()
        window.save_settings()
        assert (tmp_path / "settings.json").exists()
    finally:
        window.close()
    assert not engine.allow_ai_control


def test_terminal_ansi_clear_split_utf8_and_key_sequences():
    app = QApplication.instance() or QApplication([])
    terminal = TerminalView()
    terminal.show()
    data = "中文".encode()
    terminal.feed(b"old\x1b[2J\x1b[H" + data[:2])
    terminal.feed(data[2:])
    terminal.render()
    assert terminal.toPlainText().startswith("中文")
    assert "old" not in terminal.toPlainText()
    emitted = []
    terminal.transmit.connect(emitted.append)
    terminal.enabled_input = True
    QTest.keyClick(terminal, Qt.Key.Key_C, Qt.KeyboardModifier.ControlModifier)
    QTest.keyClick(terminal, Qt.Key.Key_Up)
    QTest.keyClick(terminal, Qt.Key.Key_Return)
    assert emitted == [b"\x03", b"\x1b[A", b"\r"]
    terminal.close()


def test_ui_waveform_and_peer_switch(engine, tmp_path):
    app = QApplication.instance() or QApplication([])
    window = MainWindow(engine, MCPService(engine, 19876), tmp_path / "settings.json")
    window.show()
    try:
        window.start_demo()
        pump(app, lambda: len(window.current_pane.instruments[0].curves) == 3)
        assert set(window.current_pane.instruments[0].curves) == {"sin", "cos", "ramp"}
        window.port.setCurrentText("loop://")
        window.connect()
        pump(app, lambda: engine.get(window.selected_id).state == "connected")
        assert not window.current_pane.instruments
    finally:
        window.close()


def test_monitor_merges_split_ansi_lines_without_blank_rows_and_preserves_raw(engine):
    app = QApplication.instance() or QApplication([])
    monitor = MonitorView()
    monitor.show()
    chunks = [b"\x1b", b"[0;32mI (1918) wifi_sta: try to ", b"connect...\x1b[0m\r", b"\n", b"\x1b", b"[0;33mW (14918) time_sync: timeout\x1b[0m\r\n"]
    try:
        for chunk in chunks:
            event = engine.journal.append("fixture", "rx", chunk)
            monitor.feed_event(event, "utf-8")
            app.processEvents()
        text = monitor.toPlainText()
        assert text.count("RX") == 2
        assert len(text.splitlines()) == 2
        assert "wifi_sta: try to connect..." in text
        assert "time_sync: timeout" in text
        assert all(control not in text for control in ("\x1b", "[0;32m", "␍", "␊", "\ufffd"))
        assert monitor.document().blockCount() == 2
        raw = b"".join(bytes.fromhex(event["hex"]) for event in engine.journal.snapshot())
        assert raw == b"".join(chunks)
        monitor.clear()
        for event in engine.journal.snapshot():
            monitor.feed_event(event, "utf-8", hex_mode=True)
        assert "1b" in monitor.toPlainText()
        assert monitor.document().blockCount() == len(chunks)
    finally:
        monitor.close()


def test_monitor_keeps_interleaved_sources_separate_and_filters_whole_lines(engine):
    app = QApplication.instance() or QApplication([])
    monitor = MonitorView()
    try:
        for peer, data in [("peer-a", b"left"), ("peer-b", b"right\n"), ("peer-a", b" done\n")]:
            monitor.feed_event(engine.journal.append("fixture", "rx", data, peer=peer), "utf-8")
        rows = monitor.toPlainText().splitlines()
        assert len(rows) == 2
        assert "[peer-a]  left done" in rows[0]
        assert "[peer-b]  right" in rows[1]
        monitor.clear()
        for data in (b"need", b"le\r", b"\nother\n"):
            monitor.feed_event(engine.journal.append("fixture", "rx", data), "utf-8", search="needle")
        assert monitor.toPlainText().count("RX") == 1
        assert "needle" in monitor.toPlainText()
        assert "other" not in monitor.toPlainText()
    finally:
        monitor.close()


def test_shared_terminal_switch_hides_sender_and_transmits_keys(engine, tmp_path):
    app = QApplication.instance() or QApplication([])
    window = MainWindow(engine, MCPService(engine, 19876), tmp_path / "settings.json")
    window.show()
    try:
        window.port.setCurrentText("loop://")
        window.connect()
        pump(app, lambda: engine.get(window.selected_id).state == "connected")
        channel = engine.get(window.selected_id)
        channel.receive(b"\x1b[2J\x1b[Hdevice> ")
        pump(app, lambda: "device>" in window.current_pane.console.toPlainText())
        snapshot = engine.journal.snapshot(channel.id)
        QTest.mouseClick(window.current_pane.terminal_mode, Qt.MouseButton.LeftButton)
        pump(app, lambda: window.current_pane.terminal.hasFocus())
        assert window.current_pane.display_stack.currentWidget() is window.current_pane.terminal
        assert not window.current_pane.sender_panel.isVisible()
        assert not window.current_pane.hex_view.isEnabled()
        assert not window.current_pane.timestamps.isEnabled()
        assert "device>" in window.current_pane.terminal.toPlainText()
        assert window.current_pane.terminal.enabled_input
        QTest.keyClick(window.current_pane.terminal, Qt.Key.Key_Tab)
        QTest.keyClick(window.current_pane.terminal, Qt.Key.Key_Up)
        QTest.keyClick(window.current_pane.terminal, Qt.Key.Key_C, Qt.KeyboardModifier.ControlModifier)
        pump(app, lambda: channel.tx_bytes == 5)
        sent = b"".join(bytes.fromhex(event["hex"]) for event in engine.journal.snapshot(channel.id) if event["direction"] == "tx")
        assert sent == b"\t\x1b[A\x03"
        assert window.current_pane.terminal.hasFocus()
        QTest.mouseClick(window.current_pane.terminal_mode, Qt.MouseButton.LeftButton)
        assert window.current_pane.sender_panel.isVisible()
        assert window.current_pane.display_stack.currentWidget() is window.current_pane.console
        assert "device>" in window.current_pane.console.toPlainText()
        assert engine.journal.snapshot(channel.id)[:len(snapshot)] == snapshot
    finally:
        window.close()


def test_appearance_persists_and_timestamp_toggle_preserves_clear_and_raw(engine, tmp_path):
    app = QApplication.instance() or QApplication([])
    path = tmp_path / "settings.json"
    window = MainWindow(engine, MCPService(engine, 19876), path)
    window.show()
    try:
        window.port.setCurrentText("loop://")
        window.connect()
        pump(app, lambda: engine.get(window.selected_id).state == "connected")
        channel = engine.get(window.selected_id)
        channel.receive("中文日志\r\n".encode())
        pump(app, lambda: "中文日志" in window.current_pane.console.toPlainText())
        snapshot = engine.journal.snapshot(channel.id)
        window.current_pane.timestamps.setChecked(False)
        pump(app, lambda: "中文日志" in window.current_pane.console.toPlainText())
        assert window.current_pane.console.toPlainText().splitlines()[-1] == "RX  中文日志"
        window.theme_selector.setCurrentIndex(1)
        window.set_display_font(QFont("Consolas", 15))
        assert window.theme_name == "dark"
        assert window.current_pane.console.font().pointSize() == 15
        assert window.current_pane.terminal.font().family() == "Consolas"
        assert window.current_pane.payload.font().pointSize() == 15
        assert engine.journal.snapshot(channel.id) == snapshot
        window.current_pane.clear_view()
        window.current_pane.timestamps.setChecked(True)
        window.tick()
        assert not window.current_pane.console.toPlainText()
        channel.receive(b"new line\n")
        pump(app, lambda: "new line" in window.current_pane.console.toPlainText())
        assert "Z RX" in window.current_pane.console.toPlainText()
        assert "中文日志" not in window.current_pane.console.toPlainText()
        window.current_pane.terminal_mode.setChecked(True)
    finally:
        window.close()
    reopened = MainWindow(engine, MCPService(engine, 19876), path)
    try:
        assert reopened.theme_name == "dark"
        assert reopened.current_pane.timestamps.isChecked()
        assert reopened.display_font.family() == "Consolas"
        assert reopened.font_size.value() == 15
        assert reopened.current_pane.terminal_mode.isChecked()
        assert reopened.current_pane.sender_panel.isHidden()
    finally:
        reopened.close()


def test_connection_button_tracks_connect_disconnect_and_reconnect(engine, tmp_path):
    app = QApplication.instance() or QApplication([])
    window = MainWindow(engine, MCPService(engine, 19876), tmp_path / "settings.json")
    window.show()
    try:
        window.tick()
        assert window.connect_button.text() == "建立连接"
        assert window.port.currentText() == "NULL"
        window.port.setCurrentText("loop://")
        QTest.mouseClick(window.connect_button, Qt.MouseButton.LeftButton)
        pump(app, lambda: window.connect_button.text() == "断开连接")
        first = window.selected_id
        assert window.connect_button.property("connectionState") == "active"
        QTest.mouseClick(window.connect_button, Qt.MouseButton.LeftButton)
        pump(app, lambda: window.connect_button.text() == "重新连接")
        assert engine.get(first).state == "closed"
        QTest.mouseClick(window.connect_button, Qt.MouseButton.LeftButton)
        pump(app, lambda: window.selected_id == first and window.connect_button.text() == "断开连接")
        window.protocol.setCurrentIndex(3)
        window.tick()
        assert window.connect_button.text() == "建立新连接"
    finally:
        window.close()


def test_dropdown_and_switch_animation_can_be_interrupted(engine, tmp_path):
    app = QApplication.instance() or QApplication([])
    window = MainWindow(engine, MCPService(engine, 19876), tmp_path / "settings.json")
    window.show()
    try:
        window.advanced_button.setChecked(True)
        app.processEvents()
        window.dtr.setChecked(True)
        QTest.qWait(40)
        assert 0 < window.dtr.position < 1
        window.dtr.setChecked(False)
        pump(app, lambda: window.dtr.position == 0)
        window.port.showPopup()
        pump(app, lambda: window.port.rotation == 180)
        assert window.port.view().isVisible()
        window.port.hidePopup()
        pump(app, lambda: window.port.rotation == 0)
        assert not window.port.view().isVisible()
    finally:
        window.close()


def test_connection_button_reports_connecting_and_error_without_hardware(engine, tmp_path, monkeypatch):
    app = QApplication.instance() or QApplication([])
    release = threading.Event()

    def fail_open(channel):
        release.wait(2)
        raise OSError("simulated transport error")

    monkeypatch.setattr(Channel, "open", fail_open)
    window = MainWindow(engine, MCPService(engine, 19876), tmp_path / "settings.json")
    window.show()
    try:
        window.port.setCurrentText("loop://")
        window.connection_action()
        window.tick()
        assert window.connect_button.text() == "连接中… · 取消"
        release.set()
        pump(app, lambda: window.connect_button.text() == "重新连接")
        assert window.create_connection_menu(window.selected_id).actions()[1].isEnabled()
    finally:
        release.set()
        window.close()


def test_ctrl_wheel_updates_one_font_size_for_all_views_without_units(engine, tmp_path):
    app = QApplication.instance() or QApplication([])
    window = MainWindow(engine, MCPService(engine, 19876), tmp_path / "settings.json")
    window.show()

    def wheel(view, delta, control=True):
        event = QWheelEvent(
            QPointF(20, 20), QPointF(view.viewport().mapToGlobal(QPoint(20, 20))),
            QPoint(), QPoint(0, delta), Qt.MouseButton.NoButton,
            Qt.KeyboardModifier.ControlModifier if control else Qt.KeyboardModifier.NoModifier,
            Qt.ScrollPhase.NoScrollPhase, False,
        )
        app.sendEvent(view.viewport(), event)
        app.processEvents()

    try:
        assert window.font_size.suffix() == ""
        wheel(window.current_pane.console, 240)
        assert window.font_size.value() == 13
        window.current_pane.terminal_mode.setChecked(True)
        wheel(window.current_pane.terminal, -120)
        assert window.font_size.value() == 12
        window.current_pane.terminal_mode.setChecked(False)
        wheel(window.current_pane.payload, 120)
        assert window.font_size.value() == 13
        for view in (window.current_pane.console, window.current_pane.terminal, window.current_pane.payload):
            assert view.font().pointSize() == 13
            assert view.document().defaultFont().pointSize() == 13
        wheel(window.current_pane.console, 120, control=False)
        assert window.font_size.value() == 13
        window.font_size.setValue(32)
        wheel(window.current_pane.console, 120)
        assert window.font_size.value() == 32
    finally:
        window.close()


def test_monitor_and_terminal_render_ansi_foreground_and_background(engine):
    app = QApplication.instance() or QApplication([])
    monitor = MonitorView()
    terminal = TerminalView()
    monitor.show_timestamps = False
    try:
        for chunk in (b"\x1b", b"[0;32mgreen\x1b[0m plain \x1b[38;5;196;48;2;12;34;56mred\x1b[0m\r\n"):
            monitor.feed_event(engine.journal.append("color", "rx", chunk), "utf-8")
            terminal.feed(chunk)
        terminal.render()
        app.processEvents()
        cursor = monitor.document().find("green")
        assert cursor.charFormat().foreground().color().name() == ansi_color(2)
        cursor = monitor.document().find("plain")
        assert cursor.charFormat().foreground().color().name() == monitor.colors["text"]
        cursor = monitor.document().find("red")
        assert cursor.charFormat().foreground().color().name() == "#ff0000"
        assert cursor.charFormat().background().color().name() == "#0c2238"
        formats = terminal.document().firstBlock().layout().formats()
        assert any(item.format.foreground().color().name() == ansi_color(2) for item in formats)
        assert any(item.format.foreground().color().name() == "#ff0000" for item in formats)
        assert b"\x1b[0;32m" in b"".join(bytes.fromhex(event["hex"]) for event in engine.journal.snapshot())
    finally:
        monitor.close()
        terminal.close()


def test_recovery_marker_resets_partial_decoders_and_waiting_connection_can_be_closed(engine, tmp_path):
    app = QApplication.instance() or QApplication([])
    window = MainWindow(engine, MCPService(engine, 19876), tmp_path / "settings.json")
    window.show()
    try:
        window.port.setCurrentText("loop://")
        window.connect()
        pump(app, lambda: engine.get(window.selected_id).state == "connected")
        channel = engine.get(window.selected_id)
        channel.receive(b"old\x1b[")
        pump(app, lambda: "old" in window.current_pane.console.toPlainText())
        channel.status("connected", "recovered", stream_reset=True)
        channel.receive(b"\x1b[32mnew boot\x1b[0m\n")
        pump(app, lambda: "new boot" in window.current_pane.console.toPlainText())
        assert "32m" not in window.current_pane.console.toPlainText()
        assert "old" in window.current_pane.console.toPlainText()
        channel.status("reconnecting", "simulated reset")
        window.tick()
        assert window.connect_button.text() == "等待恢复… · 断开"
        assert window.create_connection_menu(window.selected_id).actions()[0].isEnabled()
        window.connection_action()
        pump(app, lambda: channel.state == "closed")
    finally:
        window.close()
