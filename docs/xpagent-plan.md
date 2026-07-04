# xpagent Plan

`xpagent` is the XP-native side of Agentic WinXP: a small portable agent shell
that can run on Windows XP first, and other old or constrained systems later.

## Goals

- Let XP initiate conversations with a host-side agent gateway.
- Keep XP away from modern TLS, certificates, SDKs, and direct model APIs.
- Keep the core library portable C with narrow platform adapters.

## Shape

```text
portable/
  agent_core.c/.h      agent loop, transcript shape, transport callbacks
  ag_protocol.md       plain framing used between guest and host

host/
  agent_gateway.py     local echo/model gateway for XP

guest/
  src/xpagent.c        Windows XP CLI adapter
  src/xpagent-gui.c    native Win32 GUI prototype
```

## Layers

- Core: message state, send/receive flow, protocol framing.
- Transport adapter: TCP connect/read/write.
- UI adapter: console prompt/output now; TUI and Win32 GUI later.
- Tool adapter: file and process operations, initially host-mediated or explicit.
  The first implementation is host-mediated through the existing `xpilot` API.

## Milestones

1. Echo round trip through the gateway from XP.
2. Host gateway shells out to local `codex exec`.
3. Host gateway exposes XP tools through `xpilot`: run, read, write, list.
4. Approval/logging loop for tool calls.
5. Native Win32 GUI.
6. TUI or richer GUI interaction model.

The current milestone is the first three bullets: XP can call the host gateway,
the host gateway can use local Codex as the LLM backend, and Codex can operate
XP through a small `xpilot`-backed tool loop.
