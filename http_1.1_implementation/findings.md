# Findings

## Message Format (Server & Client)

---
start-line CRLF
* (header-line CRLF)
CRLF
[Message body]
---

`start-line` => request-line / status-line
