import json

from serial_assistant.structured import StructuredDecoder
from serial_assistant.engine import ConnectionConfig
from conftest import eventually


def records(parser):
    return parser.read(limit=500)["records"]


def test_extracts_split_unicode_nested_json_and_xml_from_colored_logs():
    parser = StructuredDecoder()
    parser.configure("auto")
    data = '\x1b[32mI (100) telemetry: {"中文":[1,{"text":"} ] \\\""}],"ok":true}\x1b[0m\r\nnoise <state><t unit="c">25.6</t><empty/></state> tail'.encode()
    for index in range(0, len(data), 2):
        parser.feed(data[index:index + 2], index)
    found = records(parser)
    assert [item["format"] for item in found] == ["json", "xml"]
    assert found[0]["value"]["ok"] is True
    assert "中文" in found[0]["pretty"]
    assert '<t unit="c">25.6</t>' in found[1]["pretty"]
    assert "\x1b" not in found[0]["text"]


def test_modes_reset_half_frames_without_erasing_old_records():
    parser = StructuredDecoder()
    parser.configure("json")
    parser.feed(b'prefix {"a":1}\n<state/>\n{"half":', 1)
    assert len(records(parser)) == 1
    parser.reset()
    parser.feed(b'{"boot":2}\n', 2)
    assert len(records(parser)) == 2
    parser.configure("xml")
    parser.feed(b'{"ignored":3}\n<new/>\n', 3)
    assert [item["format"] for item in records(parser)] == ["json", "json", "xml"]
    parser.configure("off")
    parser.feed(b'{"disabled":4}', 4)
    assert len(records(parser)) == 3


def test_malformed_records_cdata_comments_entities_and_limits():
    parser = StructuredDecoder()
    parser.configure("auto")
    parser.feed(b'[INFO] bad {oops}\n{"good":1}<bad></wrong><good><![CDATA[<ignored/>]]><!-- x --></good>', 1)
    found = records(parser)
    assert [item["format"] for item in found] == ["json", "xml"]
    assert found[0]["value"] == {"good": 1}
    parser.feed(b'<unsafe>&undefined;</unsafe><root>ok</root>', 2)
    assert records(parser)[-1]["text"] == '<root>ok</root>'
    parser.feed(b'{' * 200000, 3)
    assert len(parser.pending) <= parser.MAX_PENDING
    parser.reset()
    parser.feed(b'{"n":1}\n' * 510, 4)
    assert len(parser.records) == 500
    assert parser.read(after=0)["gap"]
    cursor = parser.read(after=parser.sequence - 2)["next_cursor"]
    assert parser.read(after=cursor)["records"] == []


def test_parser_has_no_effect_until_enabled_and_keeps_each_network_peer_separate(engine):
    info = engine.connect(ConnectionConfig(kind="udp", bind_port=0))
    channel = engine.get(info["id"])
    eventually(lambda: channel.state == "connected")
    channel.receive(b'{"ignored":1}\n')
    assert engine.structured(channel.id)["sources"] == {}
    engine.configure_parser(channel.id, "auto")
    channel.receive(b'{"left":', peer="a")
    channel.receive(b'<right/>', peer="b")
    channel.receive(b'1}', peer="a")
    result = engine.structured(channel.id)
    assert result["sources"]["a"]["records"][0]["value"] == {"left": 1}
    assert result["sources"]["b"]["records"][0]["format"] == "xml"
    cursors = {source: batch["next_cursor"] for source, batch in result["sources"].items()}
    assert all(not batch["records"] for batch in engine.structured(channel.id, cursors=cursors)["sources"].values())
    assert any(event["text"] == '{"left":' for event in engine.journal.snapshot(channel.id))
