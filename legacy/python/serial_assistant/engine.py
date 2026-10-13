# SPDX-License-Identifier: Apache-2.0

"""Single transport owner; GUI and MCP share this engine without consuming reads."""

from concurrent.futures import ThreadPoolExecutor
from dataclasses import asdict, dataclass
import math
import select
import socket
import threading
import time
import uuid

import serial
from serial.tools import list_ports

from .data import EventJournal, WaveDecoder, encode_payload
from .runtime import serial_open_error
from .structured import StructuredDecoder


@dataclass
class ConnectionConfig:
    kind: str = "serial"
    name: str = ""
    port: str = "NULL"
    baudrate: int = 115200
    bytesize: int = 8
    parity: str = "N"
    stopbits: float = 1
    flow: str = "none"
    dtr: bool = False
    rts: bool = False
    host: str = "127.0.0.1"
    network_port: int = 9000
    bind_host: str = "127.0.0.1"
    bind_port: int = 9001

    def validate(self):
        if self.kind not in {"serial", "tcp-client", "tcp-server", "udp", "demo"}:
            raise ValueError("不支持的协议")
        if self.kind == "serial" and (not self.port.strip() or self.port.strip().casefold() == "null"):
            raise ValueError("请先选择串口端口，NULL 表示尚未选择。")
        if not 1 <= self.baudrate <= 12000000:
            raise ValueError("波特率需要在 1..12000000 范围内，实际支持取决于驱动")
        if self.bytesize not in (5, 6, 7, 8) or self.parity not in ("N", "E", "O", "M", "S"):
            raise ValueError("串口数据位或校验位无效")
        if self.stopbits not in (1, 1.5, 2) or self.flow not in ("none", "rtscts", "xonxoff"):
            raise ValueError("串口停止位或流控无效")
        if not 0 <= self.bind_port <= 65535 or not 1 <= self.network_port <= 65535:
            raise ValueError("网络端口无效")


class Channel:
    SERIAL_RETRY_DELAY = 0.3

    def __init__(self, engine, config):
        self.engine = engine
        self.config = config
        self.id = uuid.uuid4().hex[:10]
        self.name = config.name or f"{config.kind} / {config.port if config.kind == 'serial' else config.network_port}"
        self.state = "connecting"
        self.error = ""
        self.rx_bytes = 0
        self.tx_bytes = 0
        self.handle = None
        self.peers = {}
        self.lock = threading.RLock()
        self.write_lock = threading.Lock()
        self.stop = threading.Event()
        self.serial_fault = threading.Event()
        self.serial_fault_message = ""
        self.serial_generation = 0
        self.reconnect_attempts = 0
        self.decoders = {}
        self.parsers = {}
        self.parser_mode = "off"
        self.parser_encoding = "utf-8"
        self.thread = threading.Thread(target=self.run, name=f"channel-{self.id}", daemon=True)
        self.started_at = time.monotonic()
        self.local_address = ""

    def describe(self):
        with self.lock:
            return {
                "id": self.id, "name": self.name, "state": self.state,
                "error": self.error, "config": asdict(self.config),
                "rx_bytes": self.rx_bytes, "tx_bytes": self.tx_bytes,
                "peers": list(self.peers), "local_address": self.local_address,
                "serial_generation": self.serial_generation,
                "reconnect_attempts": self.reconnect_attempts,
                "parser_mode": self.parser_mode,
            }

    def restart(self, config=None):
        if self.thread.is_alive():
            raise ValueError("连接仍在关闭，请稍后重试")
        with self.lock:
            if config is not None:
                self.config = config
                if config.name:
                    self.name = config.name
            self.stop.clear()
            self.serial_fault.clear()
            self.error = ""
            for decoder in self.decoders.values():
                decoder.pending.clear()
                decoder.discarding = False
            for parser in self.parsers.values():
                parser.reset()
            self.local_address = ""
            self.thread = threading.Thread(target=self.run, name=f"channel-{self.id}", daemon=True)
            self.status("connecting", stream_reset=True)
            self.thread.start()
            return self.describe()

    def status(self, state, message="", stream_reset=False):
        with self.lock:
            self.state = state
            if state in ("error", "reconnecting"):
                self.error = message
            elif state in ("connected", "listening"):
                self.error = ""
        self.engine.journal.append(self.id, "status", message=message or state, stream_reset=stream_reset)

    def receive(self, data, peer=""):
        with self.lock:
            self.rx_bytes += len(data)
            # Bound source state even if UDP receives packets from many addresses.
            if peer not in self.decoders and len(self.decoders) >= 32:
                del self.decoders[next(iter(self.decoders))]
            decoder = self.decoders.setdefault(peer, WaveDecoder())
            decoder.feed(data)
            event = self.engine.journal.append(self.id, "rx", data, peer=peer)
            if self.parser_mode != "off":
                if peer not in self.parsers and len(self.parsers) >= 32:
                    del self.parsers[next(iter(self.parsers))]
                parser = self.parsers.setdefault(peer, StructuredDecoder())
                parser.configure(self.parser_mode, self.parser_encoding)
                parser.feed(data, event["seq"], peer)

    def run(self):
        try:
            if self.config.kind == "serial":
                self.run_serial()
                return
            self.open()
            if self.stop.is_set():
                return
            self.status("listening" if self.config.kind == "tcp-server" else "connected")
            if self.config.kind == "demo":
                self.run_demo()
            else:
                self.run_network()
        except Exception as exc:
            if not self.stop.is_set():
                message = serial_open_error(exc) if self.config.kind == "serial" else str(exc)
                self.status("error", message)
        finally:
            self.cleanup()
            if self.state != "error":
                self.status("closed")

    def run_serial(self):
        established = False
        while not self.stop.is_set():
            try:
                if established:
                    with self.lock:
                        self.reconnect_attempts += 1
                self.serial_fault.clear()
                self.open()
                if self.stop.is_set():
                    return
                recovered = established
                established = True
                with self.lock:
                    self.serial_generation += 1
                    if recovered:
                        for decoder in self.decoders.values():
                            decoder.pending.clear()
                            decoder.discarding = False
                        for parser in self.parsers.values():
                            parser.reset()
                self.status("connected", "串口已恢复，继续接收。" if recovered else "connected", stream_reset=recovered)
                while not self.stop.is_set():
                    if self.serial_fault.is_set():
                        raise serial.SerialException(self.serial_fault_message)
                    data = self.handle.read(min(max(self.handle.in_waiting, 1), 65536))
                    if data:
                        self.receive(data)
                return
            except (serial.SerialException, OSError) as exc:
                if self.stop.is_set():
                    return
                if not established:
                    raise
                message = serial_open_error(exc)
                if self.state != "reconnecting":
                    self.status("reconnecting", f"串口暂时不可用，正在等待 {self.config.port} 恢复；可手动断开停止等待。{message}")
                else:
                    with self.lock:
                        self.error = message
                self.cleanup()
                if self.stop.wait(self.SERIAL_RETRY_DELAY):
                    return

    def open(self):
        cfg = self.config
        if cfg.kind == "serial":
            # loop:// is the only virtual serial URL we expose; COM paths are otherwise native.
            if "://" in cfg.port and cfg.port != "loop://":
                raise ValueError("串口地址仅支持本机端口或 loop://")
            handle = serial.serial_for_url(cfg.port, do_not_open=True)
            self.handle = handle
            handle.baudrate = cfg.baudrate
            handle.bytesize = cfg.bytesize
            handle.parity = cfg.parity
            handle.stopbits = cfg.stopbits
            handle.timeout = 0.05
            handle.write_timeout = 1
            handle.rtscts = cfg.flow == "rtscts"
            handle.xonxoff = cfg.flow == "xonxoff"
            handle.dtr = cfg.dtr
            handle.rts = cfg.rts
            handle.open()
        elif cfg.kind == "tcp-client":
            self.handle = socket.create_connection((cfg.host, cfg.network_port), timeout=3)
            self.handle.settimeout(0.2)
            self.local_address = str(self.handle.getsockname())
        elif cfg.kind in ("tcp-server", "udp"):
            self.handle = socket.socket(socket.AF_INET, socket.SOCK_DGRAM if cfg.kind == "udp" else socket.SOCK_STREAM)
            self.handle.bind((cfg.bind_host, cfg.bind_port))
            self.local_address = str(self.handle.getsockname())
            if cfg.kind == "tcp-server":
                self.handle.listen(16)
            self.handle.settimeout(0.2)

    def run_network(self):
        while not self.stop.is_set():
            with self.lock:
                peers = dict(self.peers)
            sockets = [self.handle, *peers.values()]
            ready, _, _ = select.select(sockets, [], [], 0.1)
            for handle in ready:
                if handle is self.handle and self.config.kind == "tcp-server":
                    client, address = handle.accept()
                    client.settimeout(1)
                    peer = f"{address[0]}:{address[1]}"
                    with self.lock:
                        if len(self.peers) >= 16:
                            client.close()
                            continue
                        self.peers[peer] = client
                    self.engine.journal.append(self.id, "status", message="TCP 客户端接入", peer=peer)
                elif self.config.kind == "udp":
                    data, address = handle.recvfrom(65535)
                    self.receive(data, f"{address[0]}:{address[1]}")
                else:
                    peer = next((key for key, value in peers.items() if value is handle), "")
                    try:
                        data = handle.recv(65536)
                    except (ConnectionError, OSError):
                        if handle is self.handle:
                            raise
                        data = b""
                    if data:
                        self.receive(data, peer)
                    elif handle is self.handle:
                        return
                    else:
                        with self.lock:
                            self.peers.pop(peer, None)
                            self.decoders.pop(peer, None)
                        handle.close()
                        self.engine.journal.append(self.id, "status", message="TCP 客户端断开", peer=peer)

    def run_demo(self):
        while not self.stop.wait(0.05):
            elapsed = time.monotonic() - self.started_at
            data = f"sin={math.sin(elapsed * 2):.4f},cos={math.cos(elapsed * 2):.4f},ramp={(elapsed % 4) / 2 - 1:.4f}\n"
            self.receive(data.encode())

    def send(self, data, peer="", source="gui"):
        with self.write_lock:
            with self.lock:
                if self.state not in ("connected", "listening"):
                    raise ValueError("连接尚未就绪或已经关闭")
                peers = dict(self.peers)
            if self.config.kind == "serial":
                try:
                    written = self.handle.write(data)
                except serial.SerialTimeoutException:
                    raise
                except (serial.SerialException, OSError) as exc:
                    self.serial_fault_message = str(exc)
                    self.serial_fault.set()
                    raise
                if written != len(data):
                    raise IOError(f"仅发送了 {written}/{len(data)} 字节")
            elif self.config.kind == "tcp-client":
                self.handle.sendall(data)
            elif self.config.kind == "tcp-server":
                # A server send must target one peer, never silently broadcast.
                if not peer and len(peers) == 1:
                    peer = next(iter(peers))
                if peer not in peers:
                    raise ValueError("请指定已连接的 TCP 客户端地址")
                peers[peer].sendall(data)
            elif self.config.kind == "udp":
                host = self.config.host
                port = self.config.network_port
                if peer:
                    host, port_text = peer.rsplit(":", 1)
                    port = int(port_text)
                self.handle.sendto(data, (host, port))
                peer = f"{host}:{port}"
            else:
                self.receive(data)
            with self.lock:
                self.tx_bytes += len(data)
            self.engine.journal.append(self.id, "tx", data, message=source, peer=peer)
            return {"sent_bytes": len(data), "channel_id": self.id, "peer": peer}

    def close(self):
        self.stop.set()
        if self.thread.ident is not None:
            self.thread.join(timeout=4)

    def cleanup(self):
        with self.write_lock:
            with self.lock:
                handles = [self.handle, *self.peers.values()]
                self.handle = None
                self.peers.clear()
            for handle in handles:
                if handle is not None:
                    try:
                        handle.close()
                    except Exception:
                        pass


class Engine:
    def __init__(self):
        self.journal = EventJournal()
        self.channels = {}
        self.lock = threading.RLock()
        self.allow_ai_control = False
        self.executor = ThreadPoolExecutor(max_workers=4, thread_name_prefix="gui-io")

    @staticmethod
    def serial_ports():
        return [{"port": p.device, "description": p.description, "hwid": p.hwid} for p in list_ports.comports()]

    def connect(self, config):
        config.validate()
        with self.lock:
            if config.kind == "serial":
                port = config.port.strip().removeprefix("\\\\.\\").casefold()
                for existing in self.channels.values():
                    candidate = existing.config.port.strip().removeprefix("\\\\.\\").casefold()
                    if existing.config.kind == "serial" and candidate == port:
                        if existing.state in ("connecting", "connected", "reconnecting"):
                            raise ValueError(f"{config.port} 已在本程序中打开，请选择已有连接「{existing.name}」，无需重复建立连接。")
                        return existing.restart(config)
            if len(self.channels) >= 64:
                raise ValueError("最多保留 64 个连接，请先移除已关闭的连接")
            channel = Channel(self, config)
            self.channels[channel.id] = channel
            channel.thread.start()
            return channel.describe()

    def get(self, channel_id):
        with self.lock:
            if channel_id not in self.channels:
                raise ValueError("连接不存在，请先调用 list_connections")
            return self.channels[channel_id]

    def list_connections(self):
        with self.lock:
            return [channel.describe() for channel in self.channels.values()]

    def disconnect(self, channel_id):
        self.get(channel_id).close()
        return self.get(channel_id).describe()

    def reconnect(self, channel_id):
        with self.lock:
            channel = self.get(channel_id)
            if channel.state not in ("closed", "error"):
                raise ValueError("请先断开当前连接")
            return channel.restart()

    def rename(self, channel_id, name):
        channel = self.get(channel_id)
        with channel.lock:
            channel.name = name.strip()[:128] or channel.name
            channel.config.name = channel.name

    def remove(self, channel_id):
        channel = self.get(channel_id)
        channel.close()
        with self.lock:
            self.channels.pop(channel_id, None)

    def send(self, channel_id, payload, hex_mode=False, encoding="utf-8", newline="none", peer="", source="gui"):
        data = encode_payload(payload, encoding, hex_mode, newline)
        return self.get(channel_id).send(data, peer, source)

    def waveform(self, channel_id, limit=1000):
        channel = self.get(channel_id)
        with channel.lock:
            return {"channel_id": channel_id, "sources": {peer or "default": decoder.read(limit) for peer, decoder in channel.decoders.items()}}

    def configure_parser(self, channel_id, mode, encoding="utf-8"):
        validator = StructuredDecoder()
        validator.configure(mode, encoding)
        channel = self.get(channel_id)
        with channel.lock:
            channel.parser_mode = mode
            channel.parser_encoding = encoding
            for parser in channel.parsers.values():
                parser.configure(mode, encoding)

    def structured(self, channel_id, after=0, limit=100, cursors=None):
        channel = self.get(channel_id)
        with channel.lock:
            return {
                "channel_id": channel_id, "mode": channel.parser_mode,
                "sources": {peer or "default": parser.read((cursors or {}).get(peer or "default", after), limit) for peer, parser in channel.parsers.items()},
            }

    def close(self):
        self.executor.shutdown(wait=True, cancel_futures=True)
        with self.lock:
            channels = list(self.channels.values())
        for channel in channels:
            channel.close()
