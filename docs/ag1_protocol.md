# AG1 Protocol

The first `xpagent` protocol is deliberately plain TCP with length-prefixed text
frames. The host gateway can translate these frames into real model API calls.

```text
AG1 <TYPE> <ID> <LEN>\n
<LEN bytes of payload>
```

Types:

- `USER`: user message from the guest.
- `ASSISTANT`: assistant message from the host.
- `ERROR`: host-side error text.
- `BYE`: close the session.

Example:

```text
AG1 USER 1 12
hello there
AG1 ASSISTANT 1 18
echo: hello there
```

Rules:

- Payloads are bytes. Text payloads are UTF-8.
- IDs are decimal request IDs chosen by the guest.
- The core should not assume sockets, files, terminals, JSON, or Win32.
