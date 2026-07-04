# Windows XP QEMU VM

This project boots the included Windows XP SP3 x86 ISO under QEMU on Apple Silicon.
It uses full x86 emulation, so it will not be fast, but it is plenty for a small
Win32/TCC/Winsock agent.

## Quick Start

```sh
./scripts/fetch-tools.sh
./scripts/package-agent-kit.sh
./xpilot_host.py
./scripts/run-xp.sh
```

After XP login, `xpilot.exe` starts from the Startup folder and connects back to
the host bridge. Check it from macOS:

```sh
./xpilotctl.py status
./xpilotctl.py info
./xpilotctl.py shell
```

## Files

- `en_windows_xp_professional_with_service_pack_3_x86_cd_vl_x14-73974.iso` - install ISO.
- `vm/winxp.qcow2` - virtual hard disk.
- `scripts/install-xp.sh` - boot the installer from CD once.
- `scripts/run-xp.sh` - boot the installed VM from disk.
- `scripts/make-transfer-iso.sh` - package a host folder as an ISO for the guest.
- `guest/` - XP-side source, batch files, and startup wrapper.
- `xpilot_host.py` / `xpilotctl.py` - macOS-side bridge and CLI.
- `transfer/` - generated/downloaded files served to XP.
- `snapshots/` - local qcow2 baselines.
- `docs/architecture.md` - short architecture and port map.

The large VM artifacts are intentionally ignored by git. Rebuild the XP payload
with:

```sh
./scripts/fetch-tools.sh
./scripts/package-agent-kit.sh
```

## Install

Run:

```sh
./scripts/install-xp.sh
```

In the XP installer, install to the blank virtual disk. When setup reboots, do not
press a key at the "Press any key to boot from CD" prompt; let it continue from the
hard disk.

## Run After Install

Run:

```sh
./scripts/run-xp.sh
```

The QEMU Cocoa window scales to fit when resized by default. To force the old
fixed-size behavior:

```sh
./scripts/run-xp.sh --no-resize
```

## Move Files Into XP

The quickest method is to serve a folder from macOS and download files in XP:

```sh
./scripts/serve-transfer.sh
```

Put files in `transfer/`, then in XP open:

```text
http://10.0.2.2:8000/
```

The prepared TCC payload is:

```text
http://10.0.2.2:8000/agent-kit.zip
```

Download it in XP, then extract it to `C:\`. It should create:

```text
C:\tcc
C:\agent
```

Open Command Prompt in XP and run:

```bat
cd \agent
build-hello.bat
hello.exe
build-netcheck.bat
netcheck.exe
```

`netcheck.exe` should print the response from the macOS host gateway.

## Pilot XP From macOS

Start the host bridge:

```sh
./xpilot_host.py
```

In XP, after extracting the latest `agent-kit.zip`, open Command Prompt and run:

```bat
cd \agent
build-xpilot.bat
xpilot.exe
```

Once XP shows `connected`, macOS can run commands through the bridge:

```sh
./xpilotctl.py status
./xpilotctl.py ping
./xpilotctl.py info
./xpilotctl.py pwd
./xpilotctl.py cd C:\agent
./xpilotctl.py run dir C:\agent
./xpilotctl.py run -t 2 ping -n 10 127.0.0.1
```

Move and inspect files with:

```sh
./xpilotctl.py ls C:\agent
./xpilotctl.py cat C:\agent\hello.c
./xpilotctl.py stat C:\agent\xpilot.exe
./xpilotctl.py get C:\agent\hello.c /tmp/hello.c
./xpilotctl.py put /tmp/hello.c C:\agent\hello.c
printf "hello\n" | ./xpilotctl.py write C:\agent\note.txt
./xpilotctl.py append C:\agent\note.txt "again"
./xpilotctl.py edit C:\agent\hello.c
./xpilotctl.py putdir ./guest C:\agent\guest-copy
./xpilotctl.py getdir C:\agent /tmp/agent-copy
./xpilotctl.py shell
```

`xpilot.exe` prints timestamped log lines in the XP console as requests arrive,
including command previews, file paths, byte counts, exit codes, timeouts, and
disconnect/reconnect events.

To run xpilot after XP login, place `xpilot-startup.bat` in:

```text
C:\Documents and Settings\All Users\Start Menu\Programs\Startup
```

This project installs that startup file during setup. Remove it from that folder
to disable automatic startup.

For offline or installer-style transfers, create an ISO from a host folder:

```sh
./scripts/make-transfer-iso.sh ./transfer
```

Then boot XP with that ISO mounted:

```sh
WINXP_CDROM=./transfer.iso ./scripts/run-xp.sh
```

Inside XP it will appear as a CD-ROM.

## Networking

The VM uses QEMU user-mode networking with an `rtl8139` network card. XP should
detect this without extra drivers. From inside the guest, the macOS host is usually
reachable at:

```text
10.0.2.2
```

That is the address the future Win32 agent should use to reach a host-side gateway
such as `10.0.2.2:7777`. Keep XP isolated and avoid general internet browsing; the
OS is far past its support window.

Start the tiny test gateway on macOS:

```sh
./host_gateway.py
```

Then in XP, open Internet Explorer to:

```text
http://10.0.2.2:7777/health
```

If that page loads, the guest-to-host bridge is working.
