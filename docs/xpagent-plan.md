# xpagent Plan

`xpagent` is the XP-native side of Agentic WinXP: a small portable agent shell
that can run on Windows XP first, and other old or constrained systems later.

## Goals

- Let XP initiate conversations with a host-side agent gateway.
- Keep XP away from modern TLS, certificates, SDKs, and direct model APIs.
- Keep the core library portable C with narrow platform adapters.
- Support local macOS testing with the same core before compiling for XP.

## Shape

```text
portable/
  agent_core.c/.h      agent loop, transcript shape, transport callbacks
  ag_protocol.md       plain framing used between guest and host

host/
  agent_gateway.py     local echo/model gateway for XP and local tests
  xpagent_posix.c      macOS/Linux CLI adapter for fast local testing

guest/
  xpagent.c            Windows XP CLI adapter
```

## Layers

- Core: message state, send/receive flow, protocol framing.
- Transport adapter: TCP connect/read/write.
- UI adapter: console prompt/output now; TUI and Win32 GUI later.
- Tool adapter: file and process operations, initially host-mediated or explicit.

## Milestones

1. Echo round trip through the gateway from macOS.
2. Echo round trip through the gateway from XP.
3. Host gateway shells out to local `codex exec`.
4. XP exposes basic tools: read, write, list, run.
5. Approval/logging loop for tool calls.
6. TUI.
7. Win32 GUI.

The current milestone is the first three bullets: the same core runs locally on
macOS and inside XP, and the host gateway can use local Codex as the LLM backend.
