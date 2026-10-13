# SPDX-License-Identifier: Apache-2.0

from concurrent.futures import ThreadPoolExecutor
import time

import pytest

from serial_assistant.data import EventJournal, WaveDecoder, encode_payload


def test_independent_readers_pagination_and_loss():
    journal = EventJournal(max_events=4)
    for i in range(4):
        journal.append("serial", "rx", bytes([i]))
    reader_a = journal.read(limit=2)
    reader_b = journal.read(limit=2)
    assert reader_a == reader_b
    assert reader_a["has_more"]
    assert [e["seq"] for e in journal.read(reader_a["next_cursor"])["events"]] == [3, 4]
    assert len(journal.read()["events"]) == 4
    journal.append("serial", "rx", b"next")
    assert journal.read()["gap"]
    assert journal.read()["oldest_cursor"] == 2
    assert journal.read(999)["cursor_reset_required"]


def test_filtered_cursor_does_not_skip_data():
    journal = EventJournal()
    for channel in ("a", "b", "a", "b", "a"):
        journal.append(channel, "rx", b"x")
    first = journal.read(0, 1, "a")
    second = journal.read(first["next_cursor"], 1, "a")
    third = journal.read(second["next_cursor"], 1, "a")
    assert [first["events"][0]["seq"], second["events"][0]["seq"], third["events"][0]["seq"]] == [1, 3, 5]
    assert not third["has_more"]


def test_read_does_not_expose_internal_events():
    journal = EventJournal()
    journal.append("a", "rx", b"original")
    journal.read()["events"][0]["text"] = "mutated"
    assert journal.read()["events"][0]["text"] == "original"


def test_bounded_encoded_memory():
    journal = EventJournal(max_bytes=500)
    for _ in range(10):
        journal.append("a", "rx", b"\xff" * 50)
    assert journal.byte_count <= 500
    assert journal.read()["gap"]


def test_long_poll_wakes_two_readers_without_consuming():
    journal = EventJournal()
    with ThreadPoolExecutor(2) as pool:
        a = pool.submit(journal.read, 0, 200, None, 2000)
        b = pool.submit(journal.read, 0, 200, None, 2000)
        time.sleep(0.04)
        journal.append("a", "rx", b"hello")
        assert a.result() == b.result()


def test_incremental_wave_decoder_mixed_formats():
    decoder = WaveDecoder()
    for part in (b"1.0,", b"2.0\n", b"temp=25.5,v=3.3\r\n", b'{"a":2,"b":4}\n', b"[3,5]\n", b"boot message\n", b"nan,inf\n"):
        decoder.feed(part)
    result = decoder.read()
    assert result["total_samples"] == 4
    assert result["samples"][0]["values"] == {"CH1": 1, "CH2": 2}
    assert result["samples"][1]["values"] == {"temp": 25.5, "v": 3.3}
    assert result["rejected_lines"] == 2


def test_wave_oversize_line_does_not_parse_tail_as_sample():
    decoder = WaveDecoder()
    decoder.feed(b"x" * 9000)
    decoder.feed(b"1,2\n3,4\n")
    assert decoder.read()["total_samples"] == 1
    assert decoder.read()["samples"][0]["values"] == {"CH1": 3, "CH2": 4}


@pytest.mark.parametrize("line", [b'{"a":true}', b'{"a":null}', b'{"a":[]}', b"1e999", b"\xff"])
def test_wave_rejects_non_numeric(line):
    decoder = WaveDecoder()
    decoder.feed(line + b"\n")
    assert not decoder.read()["samples"]


def test_payload_encoding():
    assert encode_payload("01 03 ff", hex_mode=True) == b"\x01\x03\xff"
    assert encode_payload("AT", newline="crlf") == b"AT\r\n"
    assert encode_payload("中文", encoding="gbk") == "中文".encode("gbk")
    with pytest.raises(ValueError):
        encode_payload("0 ff", hex_mode=True)
    with pytest.raises(ValueError):
        encode_payload("")
