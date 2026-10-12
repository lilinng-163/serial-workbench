from collections import deque
import threading
import time

import pytest
import serial

from serial_assistant.engine import Channel, ConnectionConfig
from conftest import eventually


class FakeSerial:
    def __init__(self, script=(), write_error=False):
        self.script = deque(script)
        self.closed = False
        self.write_error = write_error
        self.writes = []

    def open(self):
        self.closed = False

    @property
    def in_waiting(self):
        if self.script and isinstance(self.script[0], bytes):
            return len(self.script[0])
        return 0

    def read(self, size):
        if self.script:
            value = self.script.popleft()
            if isinstance(value, Exception):
                raise value
            return value
        time.sleep(0.005)
        return b""

    def write(self, data):
        self.writes.append(data)
        if self.write_error:
            raise serial.SerialException("device reset during write")
        return len(data)

    def close(self):
        self.closed = True


def test_reset_keeps_channel_id_history_and_retries_until_port_returns(engine, monkeypatch):
    available = threading.Event()
    first = FakeSerial([b"1,2\npartial", serial.SerialException("USB removed")])
    restored = FakeSerial([b"3,4\n"])
    calls = []

    def factory(port, **kwargs):
        calls.append(port)
        if len(calls) == 1:
            return first
        if not available.is_set():
            raise serial.SerialException("device not enumerated yet")
        return restored

    monkeypatch.setattr(serial, "serial_for_url", factory)
    monkeypatch.setattr(Channel, "SERIAL_RETRY_DELAY", 0.02)
    info = engine.connect(ConnectionConfig(port="COM_FAKE_RESET"))
    channel = engine.get(info["id"])
    eventually(lambda: channel.state == "reconnecting" and len(calls) >= 3)
    assert first.closed
    assert len(engine.list_connections()) == 1
    assert channel.error
    with pytest.raises(ValueError):
        engine.connect(ConnectionConfig(port="com_fake_reset"))
    with pytest.raises(ValueError):
        engine.send(channel.id, "not queued while offline")
    cursor = engine.journal.sequence
    available.set()
    eventually(lambda: channel.state == "connected" and channel.serial_generation == 2 and channel.rx_bytes == 15)
    later = engine.journal.read(cursor, channel_id=channel.id)
    assert any(event.get("stream_reset") for event in later["events"])
    assert any(event["text"] == "3,4\n" for event in later["events"])
    assert channel.id == info["id"]
    assert not channel.error
    assert engine.waveform(channel.id)["sources"]["default"]["total_samples"] == 2
    states = [event["message"] for event in engine.journal.snapshot(channel.id) if event["direction"] == "status"]
    assert "closed" not in states
    assert len(states) == 3  # connected, waiting, recovered; failed retries do not flood logs.
    engine.disconnect(channel.id)
    count = len(calls)
    time.sleep(0.06)
    assert len(calls) == count
    assert channel.state == "closed"
    assert restored.closed


def test_manual_disconnect_cancels_wait_for_reset_device(engine, monkeypatch):
    calls = []

    def factory(port, **kwargs):
        calls.append(port)
        if len(calls) == 1:
            return FakeSerial([OSError("removed")])
        raise serial.SerialException("still absent")

    monkeypatch.setattr(serial, "serial_for_url", factory)
    monkeypatch.setattr(Channel, "SERIAL_RETRY_DELAY", 0.02)
    channel = engine.get(engine.connect(ConnectionConfig(port="COM_FAKE_CANCEL"))["id"])
    eventually(lambda: channel.state == "reconnecting" and len(calls) > 1)
    engine.disconnect(channel.id)
    count = len(calls)
    time.sleep(0.06)
    assert channel.state == "closed"
    assert not channel.thread.is_alive()
    assert len(calls) == count


def test_silent_device_keeps_original_handle_and_initial_open_errors_do_not_retry(engine, monkeypatch):
    calls = []
    handle = FakeSerial()

    def factory(port, **kwargs):
        calls.append(port)
        if port == "COM_FAKE_MISSING":
            raise serial.SerialException("first open refused")
        return handle

    monkeypatch.setattr(serial, "serial_for_url", factory)
    channel = engine.get(engine.connect(ConnectionConfig(port="COM_FAKE_IDLE"))["id"])
    eventually(lambda: channel.state == "connected")
    time.sleep(0.08)
    assert channel.state == "connected"
    assert channel.reconnect_attempts == 0
    assert not handle.closed
    missing = engine.get(engine.connect(ConnectionConfig(port="COM_FAKE_MISSING"))["id"])
    eventually(lambda: missing.state == "error")
    assert calls.count("COM_FAKE_MISSING") == 1


def test_write_device_error_recovers_without_replaying_uncertain_send(engine, monkeypatch):
    handles = [FakeSerial(write_error=True), FakeSerial([b"boot\n"])]
    calls = []

    def factory(port, **kwargs):
        calls.append(port)
        return handles[len(calls) - 1]

    monkeypatch.setattr(serial, "serial_for_url", factory)
    monkeypatch.setattr(Channel, "SERIAL_RETRY_DELAY", 0.02)
    channel = engine.get(engine.connect(ConnectionConfig(port="COM_FAKE_WRITE"))["id"])
    eventually(lambda: channel.state == "connected")
    with pytest.raises(serial.SerialException):
        engine.send(channel.id, "command")
    eventually(lambda: channel.serial_generation == 2 and channel.rx_bytes == 5)
    assert channel.state == "connected"
    assert handles[0].writes == [b"command"]
    assert handles[1].writes == []
    assert channel.tx_bytes == 0


def test_null_port_is_a_placeholder_and_never_opens_a_device(engine, monkeypatch):
    def unexpected(*args, **kwargs):
        raise AssertionError("NULL must not reach the serial driver")

    monkeypatch.setattr(serial, "serial_for_url", unexpected)
    assert ConnectionConfig().port == "NULL"
    with pytest.raises(ValueError, match="NULL"):
        engine.connect(ConnectionConfig())
