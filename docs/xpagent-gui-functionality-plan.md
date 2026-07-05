# XPAgent GUI Functionality Plan

The GUI now has the right broad shape. The next job is to replace the mock UI
with real state, real gateway traffic, and a small set of useful actions.

## First Functional Slice

This is the smallest version that makes the GUI feel alive:

1. Connect from the GUI to the host gateway at `10.0.2.2:7790`.
2. Let the prompt box and Send button submit one user message over AG1.
3. Append user and assistant messages to the transcript pane.
4. Update the status line through `connecting`, `thinking`, `done`, and `error`.
5. Include the selected permission and model values in request metadata, even if
   the host ignores them at first.

## Core Plumbing

- Add a GUI controller around the portable `agent_core`.
- Reuse the AG1 TCP protocol used by console `xpagent`.
- Run network and model work on a worker thread so the Win32 message loop never
  freezes.
- Add private window messages such as `WM_XPAGENT_REPLY`,
  `WM_XPAGENT_STATUS`, and `WM_XPAGENT_ERROR`.
- Store app state in C structs: conversation, messages, selected project,
  gateway status, edited files, running tasks, and environment info.
- Convert text between UTF-8 and the XP UI code page at the GUI boundary.

## Menu And Toolbar

- `File > New Chat` and toolbar New Chat should reset the active conversation,
  transcript, prompt, tasks, and edited files.
- `Edit > Copy` and `Edit > Select All` should act on the focused control.
- `View > Refresh` should reload project, environment, task, and changed-file
  state.
- `Tools > Reconnect` should close the current socket and reconnect to the host
  gateway.
- `Search`, `Scheduled`, and `Plugins` can stay disabled or mock until the core
  send/receive loop works.
- `Open in...` should expose context actions such as Explorer, Command Prompt,
  and eventually file/thread/log destinations.

## Left Pane

- Replace the static projects list with a project/thread list model.
- Have the host gateway provide project/thread metadata.
- Selection should load the active transcript and metadata into the center pane.
- Conversation age should be computed locally or supplied by the host.

## Account And Usage

- Fetch usage state from the host if Codex CLI or local state can expose
  it reliably.
- Clicking the badge can eventually open account/settings, but it is not needed right now

## Center Pane

- Header title should come from the active conversation.
- Status should reflect real runtime state: idle, connecting, thinking, running
  tool, failed, and completed in N seconds.
- Transcript should append user, assistant, and later tool messages.
- A read-only multiline `EDIT` control is enough for v1.
- Later, switch to `RICHEDIT` for colored roles, monospace blocks, and richer
  file references.
- Auto-scroll transcript after appended messages.

## Composer

- Send should read prompt text, append the user message, call the gateway, and
  clear the prompt after successful submission.
- Decide keyboard behavior: Ctrl+Enter sends and Enter inserts a newline, or
  Enter sends in a simpler single-line mode.
- Disable Send while a request is in flight.
- `+` should become an attachment/context menu. Useful first entries:
  current screenshot, XP file path, current directory, selected project.
- Mic can remain disabled for now until audio capture and transcription exist.

## Permission And Model Pickers

- `Full access`, `Ask first`, and `Read only` should map to a permission policy
  sent with each request.
- The host should enforce the policy for tool calls:
  - Full access: run/read/write tools are allowed.
  - Ask first: GUI must approve tool calls before execution.
  - Read only: no run/write tools.
- The model picker should send a profile string to the host gateway. The host can
  translate that to Codex CLI flags or ignore it until profiles exist.

## Right Pane

- Environment should become live state: branch, dirty file count, current cwd,
  `xpilot` connection, gateway connection, and active permission/model profile.
- Tasks should show active operations: current Codex request, XP tool calls,
  builds, long-running commands, and gateway processes.
- Edited files should come from host-side file/tool events or `git diff --stat`.
- Double-click behavior can come later: open a file in Notepad, show a diff, or
  fetch file contents through `xpilot`.

## Status Bar

- Keep this boring and useful:
  - Ready / Thinking / Error.
  - `xpilot` connected yes/no.
  - Gateway address.
  - Current cwd or thread id.
- Nonfatal errors should appear here first instead of immediately using modal
  dialogs.

## Suggested Build Order

1. Wire the GUI Send path to the host gateway.
2. Add worker-thread status and transcript append events.
3. Make permission and model picker values part of each request.
4. Add reconnect and error recovery.
5. Populate status bar and environment from real gateway state.
6. Populate tasks and edited files from tool/gateway events.
7. Add project/thread selection.
8. Upgrade transcript rendering if plain `EDIT` becomes too limiting.
