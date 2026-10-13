# SPDX-License-Identifier: Apache-2.0

import socket

import pytest

from serial_assistant.engine import Channel, ConnectionConfig
from conftest import eventually


def connect(engine, **kwargs):
    info = engine.connect(ConnectionConfig(**kwargs))
    channel = engine.get(info["id"])
    eventually(lambda: channel.state in ("connected", "listening", "error"))
    assert channel.state != "error", channel.error
    return channel


def rx_bytes(engine, channel):
    return b"".join(bytes.fromhex(e["hex"]) for e in engine.journal.snapshot(channel.id) if e["direction"] == "rx")


def test_serial_loop_text_binary_wave_and_disconnect(engine):
    channel = connect(engine, kind="serial", port="loop://", baudrate=250000)
    engine.send(channel.id, "01 03 00 ff", hex_mode=True)
    eventually(lambda: channel.rx_bytes == 4)
    assert rx_bytes(engine, channel) == b"\x01\x03\x00\xff"
    engine.send(channel.id, "\n1.25,2.5\n")
    eventually(lambda: engine.waveform(channel.id)["sources"]["default"]["total_samples"] == 1)
    assert channel.tx_bytes == 14
    engine.disconnect(channel.id)
    assert channel.state == "closed"
    with pytest.raises(ValueError):
        engine.send(channel.id, "closed")


def test_tcp_client_server_bidirectional_and_peer_disconnect(engine):
    server = connect(engine, kind="tcp-server", bind_port=0)
    port = server.handle.getsockname()[1]
    client = connect(engine, kind="tcp-client", network_port=port)
    eventually(lambda: len(server.describe()["peers"]) == 1)
    engine.send(client.id, "request", newline="lf")
    eventually(lambda: b"request\n" in rx_bytes(engine, server))
    engine.send(server.id, "response", newline="crlf")
    eventually(lambda: b"response\r\n" in rx_bytes(engine, client))
    client.close()
    eventually(lambda: not server.describe()["peers"])
    assert server.state == "listening"


def test_tcp_requires_peer_when_multiple_clients(engine):
    server = connect(engine, kind="tcp-server", bind_port=0)
    port = server.handle.getsockname()[1]
    client_a = connect(engine, kind="tcp-client", network_port=port)
    client_b = connect(engine, kind="tcp-client", network_port=port)
    eventually(lambda: len(server.describe()["peers"]) == 2)
    with pytest.raises(ValueError):
        engine.send(server.id, "no broadcast")
    peer = server.describe()["peers"][0]
    engine.send(server.id, "targeted", peer=peer)
    eventually(lambda: client_a.rx_bytes + client_b.rx_bytes == len(b"targeted"))


def test_udp_datagrams_and_reply_to_peer(engine):
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as peer:
        peer.bind(("127.0.0.1", 0))
        peer.settimeout(2)
        channel = connect(engine, kind="udp", bind_port=0, network_port=peer.getsockname()[1])
        engine.send(channel.id, "00 ff 01", hex_mode=True)
        data, source = peer.recvfrom(1024)
        assert data == b"\x00\xff\x01"
        peer.sendto(b"1,2\n", source)
        peer.sendto(b"3,4\n", source)
        eventually(lambda: channel.rx_bytes == 8)
        events = [e for e in engine.journal.snapshot(channel.id) if e["direction"] == "rx"]
        assert len(events) == 2
        assert events[0]["peer"] == f"127.0.0.1:{peer.getsockname()[1]}"


def test_failed_connection_reports_error(engine):
    info = engine.connect(ConnectionConfig(kind="serial", port="COM_NONEXISTENT_SERIAL_WORKBENCH"))
    channel = engine.get(info["id"])
    eventually(lambda: channel.state == "error")
    assert channel.error
    assert engine.journal.snapshot(channel.id)[-1]["direction"] == "status"


def test_demo_is_explicitly_marked_and_generates_wave(engine):
    channel = connect(engine, kind="demo", name="demo")
    eventually(lambda: channel.rx_bytes > 0)
    assert channel.describe()["config"]["kind"] == "demo"
    assert set(engine.waveform(channel.id)["sources"]["default"]["samples"][0]["values"]) == {"sin", "cos", "ramp"}


def test_duplicate_local_serial_connection_never_opens_device(engine, monkeypatch):
    opened = []

    def fake_run(channel):
        opened.append(channel.config.port)
        channel.status("connected")
        channel.stop.wait()
        channel.status("closed")

    monkeypatch.setattr(Channel, "run", fake_run)
    first = engine.connect(ConnectionConfig(port="COM_TEST", name="已有连接"))
    eventually(lambda: engine.get(first["id"]).state == "connected")
    for port in ("com_test", "\\\\.\\COM_TEST"):
        with pytest.raises(ValueError, match="已有连接"):
            engine.connect(ConnectionConfig(port=port))
    assert opened == ["COM_TEST"]
    engine.disconnect(first["id"])
    reopened = engine.connect(ConnectionConfig(port="com_test"))
    eventually(lambda: engine.get(reopened["id"]).state == "connected")
    assert len(opened) == 2
    assert reopened["id"] == first["id"]
    assert len(engine.list_connections()) == 1
