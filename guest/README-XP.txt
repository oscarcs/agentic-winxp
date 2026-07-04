XP agent kit
============

Expected layout after extracting agent-kit.zip to C:\:

  C:\tcc
  C:\agent

Open Command Prompt, then run:

  cd \agent
  build-hello.bat
  hello.exe

Then test guest-to-host networking:

  build-netcheck.bat
  netcheck.exe

netcheck.exe connects to:

  http://10.0.2.2:7777/health

If it prints "winxp host gateway ok", TCC, Winsock, QEMU networking, and the
host gateway are all working.

For interactive control from Codex/macOS, build and run xpilot:

  build-xpilot.bat
  xpilot.exe

Start xpilot_host.py on the Mac before running xpilot.exe in XP. The XP program
connects out to 10.0.2.2:7778 and waits for RUN/GET/PUT commands.

For upgrade builds while xpilot.exe is already running:

  build-xpilot.bat xpilot2.exe

xpilot supports command timeouts, persistent CWD, info/stat, append, larger file
transfers, and the host-side xpilotctl.py shell.

xpilot 0.3 also prints timestamped log lines in the XP console for connections,
requests, commands, file transfers, cwd changes, stat calls, timeouts, and errors.

To start xpilot whenever XP logs in, copy xpilot-startup.bat to:

  C:\Documents and Settings\All Users\Start Menu\Programs\Startup

This starts a visible console window after login. It does not run before login.

xpagent is the XP-native agent shell experiment. It connects to the host gateway
on 10.0.2.2:7790:

  build-xpagent.bat
  xpagent.exe

Start the host gateway on macOS first:

  ./host/agent_gateway.py --backend codex

The gateway keeps Codex state on the host. The XP program only speaks the small
AG1 TCP protocol and converts between the XP command prompt code page and UTF-8
at the network boundary.
