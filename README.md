# Agentic WinXP

Agentic WinXP is an experiment in making Windows XP a usable participant in
modern agent workflows.

This involves two parts:
- `xpilot`, which allows Codex or your MacOS LLM agent of choice to pilot XP.
- An agent library (WIP) to allow XP to call modern LLMs.

## What Works

- `xpilot.exe` runs inside Windows XP and connects out to the host.
- `xpilot_host.py` accepts the guest connection and exposes a local control API.
- `xpilotctl.py` gives the host a small shell for XP command and file operations.
- TinyCC builds XP-side Win32/Winsock programs directly in the guest.
- `xpilot.exe` can start automatically after XP login and prints timestamped logs.

## Current Shape

```text
macOS host
  xpilot_host.py        listens on 127.0.0.1:7778 and 127.0.0.1:7780
  xpilotctl.py          runs commands and moves files through the bridge
  QEMU                  runs the XP guest

Windows XP guest
  xpilot.exe            connects to 10.0.2.2:7778
  TinyCC                builds small Win32/Winsock tools
  C:\agent              guest-side source and utilities
```

The guest makes outbound TCP connections to the host. That keeps XP away from
modern internet requirements and avoids inbound guest networking.

## Quick Start

Prerequisites: QEMU, Python 3, and `curl`. On macOS with Homebrew:

```sh
brew install qemu
```

For an already prepared local XP disk:

```sh
./xpilot_host.py
./scripts/run-xp.sh
```

After XP logs in, the Startup-folder wrapper should launch `xpilot.exe`. Check
the bridge from macOS:

```sh
./xpilotctl.py status
./xpilotctl.py info
./xpilotctl.py shell
```

Common commands:

```sh
./xpilotctl.py run 'ver'
./xpilotctl.py ls 'C:\agent'
./xpilotctl.py cat 'C:\agent\README-XP.txt'
./xpilotctl.py put ./local.c 'C:\agent\local.c'
./xpilotctl.py get 'C:\agent\xpilot.c' /tmp/xpilot.c
./xpilotctl.py putdir ./guest 'C:\agent\guest-copy'
./xpilotctl.py getdir 'C:\agent' /tmp/agent-copy
```

## Fresh VM Setup

The repository does not include Windows XP media, VM disks, snapshots, or
generated transfer payloads. By default, `scripts/install-xp.sh` looks for:

```text
en_windows_xp_professional_with_service_pack_3_x86_cd_vl_x14-73974.iso
```

Use that filename locally, or set `WINXP_ISO` when running the install script.

Create and install the VM:

```sh
mkdir -p vm
qemu-img create -f qcow2 vm/winxp.qcow2 16G
./scripts/install-xp.sh
```

Boot it later with:

```sh
./scripts/run-xp.sh
```

The QEMU Cocoa window scales to fit by default. Use `--no-resize` for fixed-size
display behavior.

## Build The XP Payload

Download the official TinyCC archives and build the transfer zip:

```sh
./scripts/fetch-tools.sh
./scripts/package-agent-kit.sh
./scripts/serve-transfer.sh
```

From XP, open:

```text
http://10.0.2.2:8000/agent-kit.zip
```

Extract it to `C:\`, creating:

```text
C:\tcc
C:\agent
```

Then build and run the XP bridge:

```bat
cd \agent
build-xpilot.bat
xpilot.exe
```

To start it after login, copy `C:\agent\xpilot-startup.bat` to:

```text
C:\Documents and Settings\All Users\Start Menu\Programs\Startup
```

## Repository Layout

- `guest/` - XP-side C source, batch files, tests, and startup wrapper.
- `xpilot_host.py` - host bridge for the XP control connection.
- `xpilotctl.py` - host CLI and interactive shell.
- `scripts/` - QEMU, transfer, TinyCC download, and packaging helpers.
- `docs/architecture.md` - short architecture and port map.
- `host_gateway.py` - tiny legacy health endpoint used for early network tests.

Ignored local state:

- `*.iso`
- `vm/`
- `snapshots/`
- `transfer/`
- `build/`

## Goals

Near term, XP is a tool runtime for host-side agents:

```text
host agent -> xpilotctl/xpilot_host -> xpilot.exe -> cmd/files/TinyCC in XP
```

Next, XP should be able to access agents:

```text
XP program -> plain TCP/HTTP to 10.0.2.2 -> host model gateway -> modern APIs
```

That second direction is intentionally host-mediated. XP should not need modern
TLS stacks, certificate stores, SDKs, or direct internet exposure.

## Notes

Useful local address from the XP guest:

```text
10.0.2.2
```

That is QEMU user networking's usual route back to the macOS host.
