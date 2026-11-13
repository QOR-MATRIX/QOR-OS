# sdk — building apps for QOR OS

> **Status:** template and guidance. An installable SDK (QOR.Kit, Lumen and
> QOR.Services as an exported CMake package plus a simulator) is a Phase 4
> deliverable.

## Choose your integration

Start with [CONTRIBUTING.md](../CONTRIBUTING.md), the [build guide](../docs/BUILDING.md)
and [AGENTS.md](../AGENTS.md). [ROADMAP.md](../docs/ROADMAP.md) owns progress and
acceptance; the links below identify implementation entry points, not completion claims.

| Work | Source and developer guidance |
|---|---|
| Apps and Lumen components | [App template](templates/qor-app/), [add an app](../docs/DEVELOPMENT.md#add-an-app), [Kit](../kit/README.md), [Lumen](../docs/design/LUMEN.md) |
| System services, input and audio | [Service contracts](../services/interfaces/), [service API](../docs/SERVICES.md), [controllers](../docs/CONTROLLERS.md), [audio](../docs/AUDIO.md), [qorctl](../tools/qorctl/main.cpp) |
| Prism and device integration | [Prism](../prism/README.md), [architecture](../docs/ARCHITECTURE.md), [image and Q Pi](../image/README.md), [performance](../docs/PERFORMANCE.md) |
| QQ games and assets | [QQ](../products/qq/README.md), [samples](../products/qq/samples/README.md), [asset-library implementation](../products/qq/studio/assetlibrary.cpp), [Store formats](../docs/STORE.md) |
| Agents and language models | [QQ's MCP bridge](../products/qq/agent/bridge.cpp), [protocol](../products/qq/agent/mcpserver.cpp), [registered Studio tools](../products/qq/agent/studiotools.cpp), [agent tests](../products/qq/tests/tst_agent.cpp) |
| ARQADE and QOR Media | [ARQADE app](../products/arqade/qt/README.md), [games and shared rules](../products/arqade/games/README.md), [Media](../tools/qor-media/README.md), [source ownership](../docs/UPSTREAM.md) |
| QOR Launcher, core and DRC-369 | [Upstream integration boundary](#launcher-core-and-drc-369), [core pin](../core/CMakeLists.txt), [pin-update procedure](../docs/UPSTREAM.md#moving-the-cores-pin) |

## Agents and tools

QQ Studio's `qq-mcp` executable relays Model Context Protocol (MCP) messages over
standard input/output to the running Studio's local pipe. Build it through the
[QQ product procedure](../docs/BUILDING.md#6-the-products-qq-arqade-qor-media).
An MCP client launches that built executable, initializes the protocol and asks
`tools/list` for the live schemas before using `tools/call`. The bridge opens no
network listener and holds no provider key. Do not copy an old tool list as the
protocol contract; `McpServer::toolList()` supplies the registered schemas.

Reading the scene (`scene_read`, `kinds`) differs from changing entities, writing
logic, playing a scene or capturing a frame. Studio tools run with the creator's
reach; the same-user pipe is not a security sandbox. Use a trusted test project,
review changes and retain the existing project-trust and logic-execution gates.
The creator's provider key stays in the platform keychain. No assistant receives
a QOR key or grants itself authority. The known boundaries are in
[SECURITY.md](../SECURITY.md#known-limits).

`qorctl` is a separate service client: commands such as `theme`, `wifi` and
`notify` change the running session. Consult [its command reference](../docs/SERVICES.md#qorctl)
before invoking it. Developer setup, signing, publication, image writes and runner
administration are separate operations; discovering a tool does not authorise them.

## Launcher, core and DRC-369

The QOR Launcher and DRC-369 chain functionality are developed in
**demiurge-chain**, not copied into QOR OS. The current upstream source at
[`74a1014`](https://github.com/QOR-MATRIX/demiurge-chain/tree/74a1014464f85e610f929ebadf8c199c82673db7)
was checked on 13 November 2026. Start from its rules and current documentation:

- [QOR Launcher](https://github.com/QOR-MATRIX/demiurge-chain/tree/74a1014464f85e610f929ebadf8c199c82673db7/tools/qor-launcher)
  and its [Qt shell](https://github.com/QOR-MATRIX/demiurge-chain/tree/74a1014464f85e610f929ebadf8c199c82673db7/tools/qor-launcher/shell).
- [DRC-369 pallet](https://github.com/QOR-MATRIX/demiurge-chain/blob/74a1014464f85e610f929ebadf8c199c82673db7/chain/pallets/drc369/src/lib.rs)
  and [core asset client](https://github.com/QOR-MATRIX/demiurge-chain/blob/74a1014464f85e610f929ebadf8c199c82673db7/tools/qor-launcher/core/src/chain/assets.rs).
- [Agent authorisation checks](https://github.com/QOR-MATRIX/demiurge-chain/blob/74a1014464f85e610f929ebadf8c199c82673db7/tools/qor-launcher/core/src/agents.rs)
  and the [agent-link example](https://github.com/QOR-MATRIX/demiurge-chain/blob/74a1014464f85e610f929ebadf8c199c82673db7/tools/qor-launcher/src-tauri/examples/agent_link.rs).

QOR OS builds the core at `6a4822d`, whose [C bridge command dispatch](https://github.com/QOR-MATRIX/demiurge-chain/blob/6a4822d10c93694d6d12abf33a75b4df6cac918a/tools/qor-launcher/core/src/commands.rs)
exposes only `core_version`, `token_info`, `parse_amount` and `format_sparks`.
Compiling the `desktop` feature does not expose DRC-369 or agent authorisation
through that bridge. Those integrations need upstream implementation and tests,
a reviewed pin update and trusted approvals before QOR OS can use them.
The upstream root `sdk/` is frozen; its legacy DRC RPC examples are not a supported
client for the current chain. Follow the active chain/core paths above.

QOR OS 1.0 moves no money: examples stay with free assets and test CGT on the
devnet. Amounts cross the bridge as decimal strings of integer embers. The
[masterplan](../docs/ROADMAP.md#phase-4--the-ecosystem) tracks the remaining
developer integrations without inventing endpoints, capabilities or permissions.

## Template

[`templates/qor-app/`](templates/qor-app/) is a minimal app: Lumen
controls, `Theme` and `Tokens`, a `Spring`, and an `Intent`, plus a Qt
Application Manager manifest (`info.yaml`) for device packaging.

### Building it today

Until the SDK package exists, build apps **inside this repository**: add the
template as a subdirectory, link the QOR OS modules, and import their plugins:

```cmake
# CMakeLists.txt (in-tree)
add_subdirectory(sdk/templates/qor-app)
target_link_libraries(hello-lumen PRIVATE qor_kit qor_kitplugin qor_lumen_styleplugin
                                          qor_services qor_servicesplugin)
```

```cpp
Q_IMPORT_QML_PLUGIN(QOR_KitPlugin)
Q_IMPORT_QML_PLUGIN(LumenPlugin)
Q_IMPORT_QML_PLUGIN(QOR_ServicesPlugin)
```

To appear in the shell today, add the app to `QOR.Apps` instead
([DEVELOPMENT.md → Add an app](../docs/DEVELOPMENT.md#add-an-app)).

## Themes and widgets

Themes and desktop widgets ship through the QOR OS Store as signed packages.
The formats, the widget contract and the signing tool are in
[docs/STORE.md](../docs/STORE.md). Until the developer program opens, the
Store installs only QOR-MATRIX's own.

## Guidelines

- Use Lumen controls (`import QtQuick.Controls`), Kit components, `Theme`
  roles and `Tokens`. No hard-coded colours, radii or durations.
- Reach the system only through `Services` (state) and `Intents` (actions).
- Respect Reduce Motion, Reduce Transparency, keyboard focus and
  accessibility roles.
- Render on demand. See [PERFORMANCE.md](../docs/PERFORMANCE.md).
