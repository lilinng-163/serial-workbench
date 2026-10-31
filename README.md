<!-- SPDX-License-Identifier: Apache-2.0 -->
<div align="center">
  <img src="resources/brand/lightning.svg" width="88" alt="Serial Workbench logo">
  <h1>Serial Workbench</h1>
  <p><strong>Your devices. Your terminal. The same session for your AI.</strong></p>
  <p>A native desktop workbench for serial, TCP, UDP and SSH debugging, with waveform tools and shared MCP access.</p>
  <p>
    <a href="https://github.com/lilinng-163/serial-workbench/releases/latest"><img src="https://img.shields.io/github/v/release/lilinng-163/serial-workbench?style=flat-square&color=497b58" alt="Latest release"></a>
    <img src="https://img.shields.io/badge/platform-Windows_x64-497b58?style=flat-square" alt="Windows x64">
    <img src="https://img.shields.io/badge/Qt-6.9%2B-41cd52?style=flat-square&logo=qt&logoColor=white" alt="Qt 6.9 or newer">
    <img src="https://img.shields.io/badge/MCP-HTTP_%2B_STDIO-d89032?style=flat-square" alt="MCP over HTTP and STDIO">
    <a href="LICENSE"><img src="https://img.shields.io/github/license/lilinng-163/serial-workbench?style=flat-square" alt="Apache 2.0 license"></a>
  </p>
  <p><strong>English</strong> · <a href="README.zh-CN.md">简体中文</a></p>
  <p><a href="https://github.com/lilinng-163/serial-workbench/releases/latest">Download</a> · <a href="#quick-start">Quick start</a> · <a href="#ai--mcp">AI / MCP</a> · <a href="#documentation">Documentation</a> · <a href="https://github.com/lilinng-163/serial-workbench/issues">Issues</a></p>
</div>

![Waveform workspace with four live readouts](docs/images/workspace-light.png)

## Why Serial Workbench?

Keep the connection open in one application. Watch device output, inspect structured data, tune parameters and let an AI agent analyze the same session through MCP. The GUI and each AI client have independent read cursors, so reading in one place does not consume another reader's data.

Built with **Qt 6 and C++17**. The Windows application and its native MCP bridge run without installing Python or the Qt SDK.

## ✨ Features

| | What you can do |
| --- | --- |
| 🔌 **Connections** | Serial, TCP client/server, UDP and SSH; independent tabs, live port discovery and serial recovery. |
| 🖥️ **SSH terminal** | A dedicated interactive terminal with **10,000 lines of scrollback**, selection and copy, ANSI colors, IME input and remote PTY resizing. |
| 📈 **Waveforms** | Up to 16 named curves, zoom, pan, cursor inspection and four fixed-width live readouts with selectable channels. |
| 🧊 **3D models** | Import STL, OBJ and PLY; choose a connection and named data stream, bind Euler/quaternion telemetry, remap axes, adjust manual XYZ translation and model color, with smooth parameter-tab transitions. |
| 🧩 **JSON / XML** | Separate pages sharing a connection, incremental extraction, structured inspection and a JSON assistant. |
| 🕘 **Version history** | Review the latest three stable releases in Settings, download verified installers, and see the running version in the status bar. |
| 🎛️ **Serial controls** | Sliders, numeric inputs, switches, buttons and choices; groups, draggable/resizable cards that follow window width, presets and per-control automatic sending. |
| 📦 **Files & protocols** | Raw-byte and XMODEM-CRC file transfer; C-style frame scripts with persistent state and checksum helpers. |
| 🤖 **AI collaboration** | Streamable HTTP and STDIO MCP, protocol-level access switches, read-only defaults and an explicit AI control permission. |
| 🎨 **Workspace** | Detachable tabs, light/dark themes, a color picker and hex theme colors, custom backgrounds, data fonts and per-page clear actions. |

## 📸 Screenshots

<table>
  <tr>
    <td width="50%"><img src="docs/images/ssh-terminal.png" alt="Dedicated SSH terminal with scrollback"><br><strong>SSH terminal</strong> · scrollback with sample output</td>
    <td width="50%"><img src="docs/images/workspace-dark.png" alt="Dark theme receive workspace"><br><strong>Dark workspace</strong> · connections and receive logs</td>
  </tr>
  <tr>
    <td><img src="docs/images/json.png" alt="JSON data inspector"><br><strong>JSON inspector</strong> · structured demo data</td>
    <td><img src="docs/images/xml.png" alt="XML data inspector"><br><strong>XML inspector</strong> · structured demo data</td>
  </tr>
  <tr>
    <td><img src="docs/images/serial-controls.png" alt="Serial control dashboard"><br><strong>Serial controls</strong> · parameter cards and control groups</td>
    <td><img src="docs/images/ai-settings.png" alt="AI and MCP settings"><br><strong>AI / MCP</strong> · client configuration and access controls</td>
  </tr>
</table>

Screenshots are captured from the application using generated example data and local test sessions. The current application interface is in Chinese.

## 📥 Download

**[Download the latest release →](https://github.com/lilinng-163/serial-workbench/releases/latest)**

v0.15.38 fixes one-pixel seams in terminal selections and ANSI backgrounds at fractional display scales. It retains the 3D telemetry, shared MCP and Windows upgrade features from v0.15.37. [Release notes](docs/releases/0.15.38.md).

| Package | Use it when… |
| --- | --- |
| `SerialWorkbench-<version>-windows-x64-setup.exe` | You want the installer, shortcuts and an uninstall entry. Recommended. |
| `SerialWorkbench-<version>-windows-x64-portable.zip` | You want a portable folder. Extract everything and run `SerialWorkbench.exe`. |
| `SerialWorkbench-<version>-windows-x64.exe` | You want a single-file launcher that extracts and reuses a local runtime cache. |
| `SHA256SUMS.txt` | You want to verify downloaded files with `Get-FileHash -Algorithm SHA256`. |

The installer includes the runtime libraries and MCP bridge. SSH uses the Windows OpenSSH client (`ssh.exe`). The installer detects GUI/background runtimes and MCP bridges and offers to save settings, close them and continue; silent mode requires explicit `/CLOSEAPPLICATIONS` consent. Reconnect your Agent after upgrading. Installer settings are kept in `%LOCALAPPDATA%/SerialWorkbench/settings.json`; portable settings live beside the executable. See [installation and upgrades](docs/windows-installer.md).

## Quick start

1. Install or extract the application, then launch **Serial Workbench**. It does not connect to hardware automatically.
2. Choose **串口 / TCP 客户端 / TCP 服务端 / UDP / SSH**, fill in the connection parameters and connect.
3. For serial/network sessions, use the receive log and send panel, or switch to an interactive terminal. **SSH opens directly in the terminal**, with no log-mode switch or serial send panel.
4. Click **+ → 新建控件页面** to add a waveform, JSON, XML, serial-control or **3D 模型** (3D model) page. Choose its data source; several pages can share one connection.
5. Open **设置 → AI / MCP** to connect your agent to the running application.

The receive log, serial monitor window and JSON/XML record list share the same search controls: **Aa** (case sensitive), **ab** (whole word), **.*** (regular expression) and **&&** (logical matching). All switches start off. With logical matching enabled, `temperature && alarm || fault` means both `temperature` and `alarm`, or `fault` (`&&` takes precedence). Use double quotes around terms containing operators, such as `"A||B"`; quoted regex terms also work when both modes are enabled. Invalid expressions show a warning border and an explanation on hover, keeping records visible. Search settings are saved with the page/window; filtering changes only the display, including while paused, and leaves shared MCP history intact.

No device nearby? Select **Demo · 演示波形** as a waveform data source, or launch the bundled demo:

```powershell
.\SerialWorkbench.exe --demo --view waveform
```


MCP clients can call `list_search_views` and `search_logs` to use the actual GUI filters, and `get_frame_script`, `validate_frame_script`, `set_frame_script` to inspect, test and apply serial parsers. Writes require AI control permission and a current revision; unsaved GUI edits are protected. The installer preselects skill components for detected Agents; Settings → AI / MCP → Agent 接入 → 配置 skill can install or update them later. Startup and port changes check Agent MCP addresses without rewriting configuration. See [Agent integration](docs/agent-integration.md).

When changing a parser through MCP, include matching HEX `sample_chunks` in `set_frame_script` to update the GUI trial inputs too. Validate them offline first; unsaved trial edits are protected. The script editor uses the shared skill/guide workflow and no longer has a separate AI prompt window.

### Theme color

Open **Settings → Appearance → Theme color** to tint surfaces with the main color or independently choose a contrast color. Both accept the picker and `#RRGGBB` / `#RGB` (for example, `#4080E0`). Apply, Return or leaving the field saves immediately; invalid input leaves the current color intact. Buttons retain the selected fill. Links, icons, TX logs, waveforms, borders and selections adjust their brightness for readable light/dark themes, including black and white accents. The original color values are saved, with consistent detached windows and receive monitors. **Restore default theme color/contrast** restores the original palette. Background images and data fonts retain their own settings.

### Terminal and workspace shortcuts

| Action | Shortcut / interaction |
| --- | --- |
| Browse terminal history | Mouse wheel or the terminal scrollbar; hold the left button and drag past the top/bottom edge to select across pages |
| Copy / interrupt | `Ctrl+C` copies a selection; without a selection it sends the interrupt character |
| Copy / paste | `Ctrl+Shift+C` / `Ctrl+Shift+V`; right-click paste is configurable |
| Select SSH text | `Shift` + arrows; `Ctrl+Shift` + left/right selects words |
| Change data font size | `Ctrl` + mouse wheel |
| Rename a tab | Right-click → rename, or `F2` |
| Close / detach a tab | Middle-click to close; drag out or use the context menu to detach |

Terminal output keeps arriving while you browse earlier lines without pulling the view to the bottom. Typing returns to the live screen. The clear button resets the local display; shared MCP history is retained.

### Plot device values

Send one **wave/1** line per sample:

```text
@wave temperature=25.50,voltage=3.300,rpm=1200
```

```c
printf("@wave temperature=%.2f,voltage=%.3f,rpm=%d\r\n",
       (double)temperature, (double)voltage, rpm);
```

Use named fields and terminate each line with CR, LF or CRLF. Up to 16 channels can share a frame. Plain numbers, ordinary logs, JSON and script output are handled separately. See the [wave/1 protocol](docs/wave-protocol.md).

Named streams are also supported: `@wave[imu] roll=10,pitch=20,yaw=30` and `@wave[motor] rpm=1200`. Connections, peers and stream names remain separate; curves and live values show the stream name. Select `imu` in the 3D model page or pass `stream="imu"` to `read_waveform`. An empty stream name selects legacy `@wave ...` frames. Model translation and orientation update independently; manual world-space XYZ translation works without telemetry, and model colors can follow the theme or use a picker / hex input.

## CLI and background runtime

The installation includes `SerialWorkbenchCLI.exe`, sharing settings and connections with the GUI:

```powershell
.\SerialWorkbenchCLI.exe start
.\SerialWorkbenchCLI.exe status
.\SerialWorkbenchCLI.exe call get_status
.\SerialWorkbenchCLI.exe show
```

New profiles are read-only. When write/control access is authorized, `start --allow-control` explicitly enables and saves it for a new instance. Use `--settings` for an isolated profile, `call TOOL --args-file request.json` for complex arguments, and `install-skill AGENT` / `configure-agent AGENT` for setup. `stop` closes all connections in the shared instance. The STDIO bridge starts or reuses the runtime automatically; fixed HTTP URLs remain available. See the [CLI guide](skills/serial-workbench/references/cli.md) for commands and exit codes.

## AI / MCP

STDIO starts the background runtime automatically, without opening the desktop. Agent settings select this mode by default. Fixed HTTP connections remain available and require a running runtime; check the actual URL in Settings or with `SerialWorkbenchCLI status`. The HTTP examples below use the default `http://127.0.0.1:8765/mcp`.

### Connect an agent

The settings page can configure supported locally installed clients, including Codex, Claude Code, Claude Desktop, OpenCode, Gemini CLI, Pi, Hermes, OpenClaw and Grok Build. It updates the Serial Workbench entry while preserving other client settings. For manual setup:

<details>
<summary><strong>Codex</strong></summary>

```powershell
codex mcp add serial-workbench --url http://127.0.0.1:8765/mcp
```

Or add to `~/.codex/config.toml`:

```toml
[mcp_servers.serial-workbench]
url = "http://127.0.0.1:8765/mcp"
```

</details>

<details>
<summary><strong>Claude Code</strong></summary>

```powershell
claude mcp add --transport http serial-workbench http://127.0.0.1:8765/mcp
```

</details>

<details>
<summary><strong>OpenCode</strong></summary>

For a V1 `opencode.json` / `opencode.jsonc`, merge this entry:

```json
{
  "mcp": {
    "serial-workbench": {
      "type": "remote",
      "url": "http://127.0.0.1:8765/mcp",
      "enabled": true
    }
  }
}
```

For V2 configurations using `mcp.servers`, use the application's version-aware setup. See [agent integration](docs/agent-integration.md).

</details>

<details>
<summary><strong>Claude Desktop / STDIO clients</strong></summary>

Replace the executable path with your installation directory:

```json
{
  "mcpServers": {
    "serial-workbench": {
      "command": "C:/path/to/SerialWorkbench/SerialWorkbenchMCP.exe",
      "args": []
    }
  }
}
```

Without `--url`, the bridge starts or reuses the background runtime. An explicit `--url` connects only to that server. Reload the agent after editing its configuration.

</details>

The application provides **26 client adapters** and **17 skill installation targets**, including Cursor, VS Code / Copilot, Cline, Kilo, Kiro, Qoder, Zed, Antigravity and Qwen Code. Use the searchable Settings panel or `SerialWorkbenchCLI agents`. Each adapter offers configuration export; clients with unverified or version-dependent storage paths use manual import. `export-agent generic` covers other compatible MCP clients. See the [format and path reference](docs/agent-integration.md).

### What the agent can do

| Goal | MCP tools |
| --- | --- |
| Discover sessions and ports | `get_status`, `list_connections`, `list_serial_ports` |
| Read raw device output | `read_events` for one connection; `read_all_events` for mixed history |
| Inspect parsed data | `read_waveform`, `read_structured`, `read_script_results` |
| Search the GUI display | `list_search_views`, `search_logs` — share case, whole-word, regex and logical switches with the GUI |
| Inspect or test a serial parser | `get_frame_script`, `validate_frame_script` — offline tests leave the live parser unchanged |
| Apply a serial parser | `set_frame_script` — requires AI control permission and the latest revision; protects unsaved GUI edits |
| Import / configure a shared 3D page | `open_model_view`, `configure_model_view` — local STL / OBJ / PLY, telemetry mappings, color, position and camera; requires AI control |
| Inspect / capture a 3D model | `list_model_views`, `get_model_state`, `capture_model_view` — transforms, bounds, adopted frame sequences and native MCP PNG images |
| Transfer model settings between Agent sessions | `export_model_configuration`, `import_model_configuration` — versioned JSON shared with GUI import/export, with bindings, a field guide and example frame; importing requires AI control and the latest revision and never opens hardware |
| Learn the workflow / parser syntax | `read_usage_guide` |
| Send data or manage connections | `send_data`, `open_connection`, `close_connection` — requires AI control permission |

Example prompts:

> Read the last 200 events from my connected serial device and explain the startup failure. Continue with `next_cursor` so you do not analyze old output again.

> Inspect the temperature and voltage channels. Find spikes and correlate them with the raw receive events.

> Import the STL at this local path, bind only the imu stream from the selected device, then check the quaternion mapping using both state and a rendered capture.

Read [the 3D MCP guide](skills/serial-workbench/references/model3d.md) before changing bindings. A background runtime can render without opening a window, but still needs a desktop OpenGL environment. Captures and retained poses do not prove that a physical device applied a command; use the adopted frame sequences, errors and original replies.

The bundled [Serial Workbench skill](skills/serial-workbench/SKILL.md) explains connection selection, incremental reading, native search and C-style frame scripts. Agents can retrieve the guide directly with `read_usage_guide`; the installer preselects skill components for detected Agents, and Settings can update them later. AI-opened connections and applied scripts appear in the GUI. An activity notice links to call details and the audit log.

### Access and shared history

- New configurations default to read-only AI access. Sending and connection changes require **允许 AI 发送报文和控制连接**.
- Serial, TCP, UDP and SSH each have an access switch. Disabling one hides its sessions and history from MCP while local operation continues.
- The server listens on loopback without authentication. Local processes that can reach it can access the data permitted by these switches.
- History is bounded by 20,000 events or 8 MiB. Each reader keeps its own cursor; check `gap` and `session_id` when continuing. Terminal scrollback is a separate display buffer.
- In WSL, containers or remote agents, `127.0.0.1` refers to that environment. Use a client setup that can reach the Windows host.

## Documentation

Detailed guides are currently in Chinese.

| Guide | Contents |
| --- | --- |
| [Agent integration](docs/agent-integration.md) | Client formats, configuration paths and connection checks |
| [Waveform protocol](docs/wave-protocol.md) | Framing, channel names, numeric types and limits |
| [3D models and orientation](docs/model3d.md) | Mesh import, serial field bindings, Euler angles, quaternions and coordinate conversion |
| [Version history](docs/version-history.md) | Recent stable release notes and installers |
| [Serial controls](docs/serial-controls.md) | Control groups, presets, templates and automatic sending |
| [Frame scripts](docs/frame-scripts.md) | C-style parser API, validation and examples |
| [JSON assistant](docs/json-assistant.md) | Structured JSON tools |
| [Serial file transfer](docs/serial-file-transfer.md) | Raw bytes and XMODEM-CRC |
| [Live serial ports](docs/live-ports.md) | Port discovery and USB device details |
| [Windows installation](docs/windows-installer.md) | Install, upgrade, uninstall and package verification |
| [Project structure](docs/project-structure.md) | Source organization |
| [Docker development](docker/README.md) | Linux build environment |

## Build from source

Windows builds require **Qt 6.9+** (Widgets, Network, SerialPort, Core5Compat, Svg, OpenGLWidgets, Concurrent and Test), **CMake 3.21+**, Ninja and a matching C++17 compiler. The release is built with Qt 6.11.1 and MinGW 13.1. The 3D viewport requires an OpenGL 2.1 or newer graphics environment.

```powershell
git clone https://github.com/lilinng-163/serial-workbench.git
cd serial-workbench

$env:QTDIR = 'C:/Qt/6.11.1/mingw_64'
$env:MINGW_BIN = 'C:/Qt/Tools/mingw1310_64/bin'
.\scripts\build.ps1
```

The build script compiles the application, runs native tests and deploys Qt libraries to `dist/SerialWorkbenchNative`. Python is used only for optional integration tests:

```powershell
python -m venv .venv
.\.venv\Scripts\python.exe -m pip install -r tests/integration/requirements.txt
.\.venv\Scripts\python.exe tests/integration/smoke_native.py --expected-version 0.15.37
```

To create the installer, single-file launcher and portable ZIP, install NSIS and provide the matching Qt source tree for license notices:

```powershell
.\scripts\package-release.ps1 -QtSourceRoot 'C:/Qt/6.11.1/Src' -MakeNSIS makensis
```

Artifacts and SHA-256 checksums are written to `dist/releases/`. Developer/test utilities, personal settings and custom background images are excluded from the release payload.

## Contributing

Bug reports and focused pull requests are welcome. For a bug report, include the application version, Windows version, connection type, reproduction steps and a screenshot or sample frame. Redact credentials and private device data. Run the relevant native tests for code changes.

## License

[Apache-2.0](LICENSE). Bundled libraries, fonts and agent logos retain their own licenses and attributions in `licenses/` and the corresponding source directories.
