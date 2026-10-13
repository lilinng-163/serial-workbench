# SPDX-License-Identifier: Apache-2.0

import pytest

from serial_assistant.monitor import MonitorStream
from serial_assistant.ansi import TextStyle, ansi_color, sgr_style


def metadata():
    return {"time": "2026-10-11T11:47:07.330+00:00", "direction": "rx", "peer": "", "hex": "", "message": ""}


@pytest.mark.parametrize("size", [1, 2, 7, 31, 4096])
def test_esp_log_survives_all_receive_boundaries(size):
    data = "\x1b[0;32mI (1918) wifi_sta: try to connect...\x1b[0m\r\n\x1b[0;33mW (14918) time_sync: 同步超时\x1b[0m\r\n".encode()
    stream = MonitorStream()
    completed = []
    for offset in range(0, len(data), size):
        completed.extend(line.text for line in stream.feed(data[offset:offset + size], metadata()) if line.complete)
    assert completed == ["I (1918) wifi_sta: try to connect...", "W (14918) time_sync: 同步超时"]


def test_partial_prompts_are_visible_without_waiting_for_newline():
    stream = MonitorStream()
    assert not stream.feed(b"\x1b", metadata())
    assert not stream.feed(b"[0;32m", metadata())
    first = stream.feed(b"pass", metadata())[0]
    second = stream.feed(b"word: ", metadata())[0]
    assert first.text == "pass"
    assert second.text == "password: "
    assert first.number == second.number
    assert not second.complete
    final = stream.feed(b"\r", metadata())[0]
    assert final.complete
    assert not stream.feed(b"\n", metadata())


def test_osc_titles_and_incomplete_sequences_do_not_leak_into_logs():
    stream = MonitorStream()
    completed = []
    for chunk in (b"\x1b]0;title", b"\x1b", b"\\hello\x1b[0;32", b"\nnext\n"):
        completed.extend(line.text for line in stream.feed(chunk, metadata()) if line.complete)
    assert completed == ["hello", "next"]


def test_long_unterminated_lines_have_bounded_display_state():
    stream = MonitorStream(max_line=128)
    result = stream.feed(b"a" * 1000, metadata())
    assert all(len(line.text) <= 128 for line in result)
    assert len(stream.characters) < 128


def test_sgr_colors_split_across_chunks_and_reset_are_attached_to_text():
    stream = MonitorStream()
    stream.feed(b"\x1b", metadata())
    stream.feed(b"[0;32mgreen", metadata())
    result = stream.feed(b"\x1b[0m plain \x1b[1;31mred\x1b[0m\r\n", metadata())[0]
    assert result.text == "green plain red"
    assert [(span.text, span.style.fg, span.style.bold) for span in result.spans] == [
        ("green", 2, False), (" plain ", None, False), ("red", 1, True),
    ]
    following = stream.feed(b"default\n", metadata())[0]
    assert following.spans[0].style == TextStyle()


def test_extended_colors_background_and_invalid_sgr():
    style = sgr_style(TextStyle(), "38;5;196;48;2;12;34;56;4")
    assert ansi_color(style.fg) == "#ff0000"
    assert style.bg == "#0c2238"
    assert style.underline
    assert sgr_style(style, "39;49;24") == TextStyle()
    assert sgr_style(TextStyle(), "38;5;999") == TextStyle()
    assert sgr_style(TextStyle(), "bad") == TextStyle()
    assert sgr_style(TextStyle(), "38;2;1;2;3").fg == "#010203"
