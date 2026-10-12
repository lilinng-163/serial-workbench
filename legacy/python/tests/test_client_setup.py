import asyncio
import json
import tomllib

import pytest

from serial_assistant.client_setup import ClientSetup, SERVER, jsonc_load, merge_codex, merge_jsonc
from test_mcp import service


def test_jsonc_merge_preserves_comments_strings_and_unrelated_servers():
    text = '''{
      // personal settings
      "model": "provider/model,}",
      "mcp": {
        "other": {"type":"remote", "url":"http://other",}, // preserve this
        "serial-workbench": {"type":"local", "command":["old"]},
      },
      "permission": {"edit":"ask"},
    }'''
    result = merge_jsonc(text, "mcp", SERVER, {"type": "remote", "url": "http://127.0.0.1:8765/mcp"})
    data = jsonc_load(result)
    assert data["model"] == "provider/model,}"
    assert data["mcp"]["other"]["url"] == "http://other"
    assert data["permission"] == {"edit": "ask"}
    assert '// personal settings' in result
    assert '// preserve this' in result
    assert data["mcp"][SERVER]["type"] == "remote"
    assert "command" not in data["mcp"][SERVER]


@pytest.mark.parametrize("text", ['{}', '{"model":"x", // tail\n}', '{"mcp": {}}', '{"mcp":{"other":{} /*tail*/}}'])
def test_jsonc_merge_can_insert_missing_sections_and_entries(text):
    result = merge_jsonc(text, "mcp", SERVER, {"enabled": True})
    assert jsonc_load(result)["mcp"][SERVER]["enabled"]


def test_toml_merge_preserves_providers_profiles_other_servers_and_backups(tmp_path, monkeypatch):
    monkeypatch.delenv("CODEX_HOME", raising=False)
    monkeypatch.delenv("XDG_CONFIG_HOME", raising=False)
    setup = ClientSetup("http://127.0.0.1:8877/mcp", home=tmp_path, appdata=tmp_path / "AppData")
    path = setup.path(0)
    path.parent.mkdir(parents=True)
    text = '''# keep this
model = "test-model"
[mcp_servers.other]
command = "other"
[mcp_servers.serial-workbench]
command = "old"
[mcp_servers.serial-workbench.env]
OLD_VAR = "unused"
[profiles.dev]
model = "dev-model"
'''
    path.write_text(text, encoding="utf-8")
    setup.install(0)
    data = tomllib.loads(path.read_text(encoding="utf-8"))
    assert data["mcp_servers"][SERVER] == {"url": setup.url, "enabled": True}
    assert data["mcp_servers"]["other"]["command"] == "other"
    assert data["profiles"]["dev"]["model"] == "dev-model"
    backups = list(path.parent.glob("*.bak"))
    assert len(backups) == 1
    assert backups[0].read_text(encoding="utf-8") == text
    assert "# keep this" in path.read_text(encoding="utf-8")
    for index in (1, 2, 3):
        setup.install(index)
        content = jsonc_load(setup.path(index).read_text(encoding="utf-8"))
        assert SERVER in content["mcp" if index == 2 else "mcpServers"]


def test_invalid_existing_config_is_never_overwritten(tmp_path, monkeypatch):
    monkeypatch.delenv("XDG_CONFIG_HOME", raising=False)
    setup = ClientSetup("http://127.0.0.1:8765/mcp", home=tmp_path)
    path = setup.path(2)
    path.parent.mkdir(parents=True)
    original = '{"broken":'
    path.write_text(original, encoding="utf-8")
    with pytest.raises(ValueError):
        setup.install(2)
    assert path.read_text(encoding="utf-8") == original


def test_setup_checks_real_http_and_stdio_without_opening_any_device(service):
    setup = ClientSetup(service.url)
    for index in (0, 1, 2, 3):
        result = asyncio.run(asyncio.wait_for(setup.probe(index), timeout=10))
        assert "检查通过" in result
        assert "9 个工具" in result
