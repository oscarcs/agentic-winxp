# Architecture

This repo runs Windows XP as a small, isolated Win32 tool runtime under QEMU.

```text
macOS host
  qemu-system-i386
  xpilot_host.py        listens on 127.0.0.1:7778 / 7780
  xpilotctl.py          local CLI for command and file operations
  agent_gateway.py      echo or codex-backed gateway for xpagent

Windows XP guest
  xpilot.exe            connects out to 10.0.2.2:7778
  xpagent.exe           connects out to 10.0.2.2:7790
  TinyCC                builds small Win32/Winsock programs
```

The guest only makes outbound TCP connections to the host. It does not need
modern TLS, certificates, browsers, SMB, or inbound guest networking.

`xpagent` uses a tiny length-prefixed `AG1` protocol. The XP adapter transcodes
between the console code page and UTF-8 at the edge, while the portable core and
host gateway treat message bodies as UTF-8.

When the gateway runs with `--backend codex`, it shells out to local `codex exec`.
The default session file is `.state/xpagent-codex-thread.txt`, which lets new
`xpagent` connections resume the same Codex thread. Pass `--codex-new-session`
to replace it.

## Ports

- `7778`: raw `xpilot` guest-to-host control connection.
- `7780`: local macOS HTTP API used by `xpilotctl.py`.
- `7790`: `xpagent` guest-to-host agent gateway.
- `8000`: optional temporary transfer server from `scripts/serve-transfer.sh`.
- `7777`: optional old health gateway from `host_gateway.py`.

## Ignored State

The VM disk, ISO, snapshots, and transfer payloads are local state and are not
committed. Recreate the transfer payload with:

```sh
./scripts/fetch-tools.sh
./scripts/package-agent-kit.sh
```
