# AG1 Protocol

`xpagent` uses a deliberately small TCP protocol with ASCII headers and
length-prefixed payload bytes. The host gateway currently listens on port
`7790` by default; the XP console and GUI clients default to `10.0.2.2:7790`.

## Frame

```text
AG1 <TYPE> <ID> <LEN>\n
<LEN bytes of payload>
```

The header has exactly four whitespace-separated fields:

- `AG1`: protocol magic.
- `TYPE`: message type token.
- `ID`: decimal request ID.
- `LEN`: decimal payload byte length.

`LEN` payload bytes follow immediately after the newline. There is no payload
terminator and no extra delimiter between frames.

## Message Types

- `USER`: guest-to-host user message. The host replies with `ASSISTANT` or
  `ERROR` using the same ID.
- `ASSISTANT`: host-to-guest assistant response.
- `ERROR`: host-to-guest error text.
- `BYE`: guest-to-host close request. The host replies with `BYE` and closes
  the connection.
- `PROJECTS`: guest-to-host request for GUI sidebar metadata. The request body
  is empty; the host replies with `PROJECTS` using the same ID.

The console client sends `USER` frames through `ag_exchange()` and sends
`BYE` when the user types `/quit`. The GUI sends `USER` frames on its persistent
gateway socket and opens a short-lived socket for each `PROJECTS` request.

## Payloads

Payloads are bytes. Text payloads are UTF-8. The host decodes incoming text with
replacement for invalid UTF-8 before passing it to the backend.

GUI `USER` payloads may start with metadata lines followed by a blank line:

```text
XPAGENT-META permission: <ui value>
XPAGENT-META model: <ui value>

<user message>
```

The host recognizes this block only when the payload starts with
`XPAGENT-META ` and contains a `\n\n` separator. It stores metadata keys in
lowercase for the connection session and forwards only the message after the
blank line to the backend.

`PROJECTS` response payloads are newline-separated tab records:

```text
PROJECT\t<project name>\t<expanded: 0|1>
THREAD\t<project name>\t<title>\t<age>
```

The current host emits one current project row, one current thread row, then up
to nine sibling project rows. Project names and titles are sanitized to remove
tabs and newlines before they are sent.

## Limits And Parser Behavior

- The portable receiver accepts payload lengths from `0` through
  `AG_MAX_MESSAGE` (`8192`) and appends a NUL byte in memory after the payload.
- The portable receiver accepts message type tokens up to 15 bytes.
- The portable receiver reads the header into a 128-byte buffer. Generated
  headers are much shorter for the built-in message types.
- The host gateway reads header lines up to 4096 bytes and expects exactly four
  fields with `AG1` magic.
- Request IDs are chosen by the guest. Replies should echo the request ID.
- The portable core is transport-neutral: callers supply `write_all`,
  `read_line`, and `read_exact` callbacks, so the core does not assume sockets,
  files, terminals, JSON, or Win32.

## Example

This byte stream sends `hello` to the echo backend and receives its response:

```text
AG1 USER 1 5\nhelloAG1 ASSISTANT 1 21\necho from host: hello
```

The literal `\n` sequences above represent header newlines. The
`AG1 ASSISTANT` header begins immediately after byte 5 of `hello`; no separator
is inserted between frames.
