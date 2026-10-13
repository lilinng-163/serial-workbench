# SPDX-License-Identifier: Apache-2.0

import json

from PySide6.QtCore import Qt
from PySide6.QtTest import QTest
from PySide6.QtWidgets import QApplication

from serial_assistant.mcp_server import MCPService
from serial_assistant.ui import MainWindow
from test_ui import pump
from test_workspace import open_loop


def test_plus_drafts_keep_parameters_payload_and_identity_when_tabs_move(engine, tmp_path):
    app = QApplication.instance() or QApplication([])
    window = MainWindow(engine, MCPService(engine, 19876), tmp_path / "settings.json")
    window.show()
    try:
        first = window.current_pane
        window.name.setText("first draft")
        window.baud.setCurrentText("230400")
        first.payload.setPlainText("first payload")
        QTest.mouseClick(window.workspace.plus, Qt.MouseButton.LeftButton)
        second = window.current_pane
        assert window.workspace.count() == 2
        assert second is not first
        assert window.port.currentText() == "NULL"
        assert "loop://" not in [window.port.itemText(i) for i in range(window.port.count())]
        window.name.setText("second draft")
        second.payload.setPlainText("second payload")
        window.activate_pane(first)
        assert window.name.text() == "first draft"
        assert window.baud.currentText() == "230400"
        window.workspace.bar.moveTab(0, 1)
        assert window.current_pane is first
        assert window.workspace.stack.currentWidget() is first
        open_loop(window, app)
        assert window.workspace.count() == 2
        assert first.channel_id in window.panes
        assert window.current_pane is first
        window.close_tab(first)
        pump(app, lambda: first.channel_id not in window.panes)
        assert window.current_pane is second
        assert window.name.text() == "second draft"
        assert second.payload.toPlainText() == "second payload"
        window.close_tab(second)
        assert window.workspace.count() == 1
        assert window.selected_id is None
    finally:
        window.close()


def test_control_submenus_freeze_close_and_raw_history_are_independent(engine, tmp_path):
    app = QApplication.instance() or QApplication([])
    window = MainWindow(engine, MCPService(engine, 19876), tmp_path / "settings.json")
    window.show()
    try:
        pane = open_loop(window, app)
        assert pane.data_tabs.count() == 1
        menu = pane.control_button.menu()
        groups = {action.text(): action.menu() for action in menu.actions()}
        assert {"图表", "数据"} == set(groups)
        groups["图表"].actions()[0].trigger()
        first = pane.instruments[0]
        second = pane.add_instrument("waveform")
        first.freeze.setChecked(True)
        channel = engine.get(pane.channel_id)
        channel.receive(b'temperature=25.6,voltage=3.3\n')
        pump(app, lambda: len(second.curves) == 2)
        assert not first.curves
        first.freeze.setChecked(False)
        pane.data_tabs.setCurrentWidget(first)
        pump(app, lambda: len(first.curves) == 2)
        numeric = pane.add_instrument("values")
        pump(app, lambda: numeric.table.topLevelItemCount() == 2)
        rows = [numeric.table.topLevelItem(i) for i in range(2)]
        assert {item.text(1): item.text(2) for item in rows} == {"temperature": "25.6", "voltage": "3.3"}
        snapshot = engine.journal.snapshot(pane.channel_id)
        pane.close_instrument(pane.data_tabs.indexOf(second))
        pane.close_instrument(0)
        assert pane.data_tabs.widget(0) is pane.monitor_panel
        assert channel.state == "connected"
        assert engine.journal.snapshot(pane.channel_id) == snapshot
        pane.data_tabs.setCurrentIndex(0)
        assert pane.console.isVisible() and pane.sender_panel.isVisible()
        assert not first.isVisible()
    finally:
        window.close()


def test_settings_gear_keeps_workbench_and_legacy_loop_migrates_to_null(engine, tmp_path):
    app = QApplication.instance() or QApplication([])
    path = tmp_path / "settings.json"
    path.write_text(json.dumps({"connection": {"port": "loop://"}, "appearance": {"show_wave": True, "split_direction": "vertical"}}), encoding="utf-8")
    window = MainWindow(engine, MCPService(engine, 19876), path)
    window.show()
    try:
        assert window.port.currentText() == "NULL"
        assert window.current_pane.data_tabs.currentIndex() == 0
        assert window.current_pane.instruments[0].kind == "waveform"
        QTest.mouseClick(window.settings_button, Qt.MouseButton.LeftButton)
        assert window.settings_dialog.isVisible()
        window.settings_tabs.setCurrentIndex(1)
        assert window.mcp_panel.isVisible()
        window.mcp_panel.ai_control.setChecked(True)
        assert engine.allow_ai_control
        window.settings_dialog.hide()
        assert window.current_pane.console.isVisible()
        assert window.workspace.count() == 1
        window.save_settings()
        assert "split_direction" not in json.loads(path.read_text(encoding="utf-8"))["appearance"]
    finally:
        window.close()
