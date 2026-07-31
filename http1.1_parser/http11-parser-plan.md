# HTTP/1.1 Parser — Implementation Plan

A C++20 exercise: build a standalone, I/O-free HTTP/1.1 **request** parser,
correct and hardened enough to later drive a non-blocking server.

Reference spec: **[RFC 9110](https://www.rfc-editor.org/rfc/rfc9110.html)**
(HTTP Semantics) and **[RFC 9112](https://www.rfc-editor.org/rfc/rfc9112.html)**
(HTTP/1.1 wire format) — these obsolete the older RFC 7230–7235 series but
describe the same protocol. Cite these when you need to check an edge case.

---

## 1. Goals & Scope

- Parse HTTP/1.1 **requests** incrementally, byte-by-byte or in arbitrary
  chunks, exactly as they'd arrive from a non-blocking socket.
- Be a **security-relevant** component from day one: malformed or ambiguous
  input must be rejected, not "helpfully" interpreted. Most historical HTTP
  parser bugs (request smuggling, cache poisoning, DoS) come from being too
  permissive, not too strict.
- Stay **I/O-free**: no sockets, no syscalls. Bytes go in, parse events/state
  come out. This is what makes the parser unit-testable, fuzzable, and
  reusable in any server design later.
- Symmetric enough in design that a response parser could be added later,
  though this plan targets requests only.

### Explicitly out of scope (v1)

Decide and document these rather than leaving them ambiguous:

- HTTP/2, HTTP/3 (different wire format entirely)
- Request pipelining semantics (parsing multiple requests back-to-back is
  fine; scheduling/ordering them is a server concern)
- `absolute-form` / `authority-form` / `asterisk-form` request targets —
  pick one to support explicitly, or list them as future work
- 100-continue *behavior* (the parser can recognize the header; deciding
  whether to send `100 Continue` is the server's job)
- TLS, compression, content negotiation — layers above this parser

---

## 2. Core Design Decisions

### 2.1 API shape

Incremental, non-throwing, driven by a `feed()` call:

```cpp
enum class ParseStatus { NeedMoreData, Complete, Error };

class Http11RequestParser {
public:
    explicit Http11RequestParser(Limits limits = Limits::defaults());

    ParseStatus feed(const char* data, size_t len);

    // valid once Complete:
    const RequestLine&        request_line() const;
    const std::vector<HeaderField>& headers()  const;
    std::string_view          body()           const; // or streaming, see 3.6

    ParseError last_error() const;   // valid once Error
    void reset();                    // reuse buffers for the next request on a keep-alive connection
};
```

No exceptions on the hot path. Errors are values (`ParseStatus::Error` +
a `ParseError` enum/struct with enough detail to log/debug), because
exceptions on malformed network input are both slow and awkward for a
server's event loop to reason about.

### 2.2 Ownership / buffering

Two options:

| Approach | Pros | Cons |
|---|---|---|
| **Internal copy** (parser copies fed bytes into its own buffer) | Simple, safe, caller can reuse/free its buffer immediately | One extra copy per byte |
| **Zero-copy** (parser stores views into a caller-owned buffer) | No copy | Caller must guarantee buffer lifetime/stability; much easier to get wrong |

**Start with internal copy.** Get correctness and the test suite solid first;
treat zero-copy as a later optimization pass once you have a fuzzed,
passing parser to compare against.

### 2.3 State machine

Explicit, resumable at *any* byte boundary — a header line split across two
`read()`s is the normal case, not an edge case:

```
StartLine → Headers → (BodyFixed | BodyChunked | NoBody) → [Trailers] → Done
                                                                  ↓
                                                                Error (from any state)
```

Each state's parsing function must be able to return "need more data" and be
called again later with the buffer having grown, picking up exactly where it
left off (typically tracked via a cursor/offset into the internal buffer,
not by re-scanning from the start).

### 2.4 Header storage: **vector of pairs**, not a map

Use:

```cpp
struct HeaderField { std::string name, value; };
std::vector<HeaderField> headers;
```

**Why not `std::map`?** It reorders fields by key, and header field *order*
can be observable and meaningful (e.g. repeated `Set-Cookie`, or servers/
proxies that behave differently depending on header order — itself a
smuggling-adjacent concern).

**Why not `std::unordered_map` either?** Same root problem — it doesn't
solve ordering. `unordered_map` iteration order depends on hash bucket
layout, not insertion order, and can shift as the map rehashes. Two other
costs come with it:

- Header names are compared **case-insensitively** (RFC 9110 §5.1), so a
  hash map needs a custom hasher + equality predicate (`ci_hash`,
  `ci_equal`) instead of the default `std::hash<std::string>`.
- Real requests have on the order of 10–40 header fields. At that size a
  linear scan over a vector is typically *faster* than a hash map lookup
  due to cache locality — no bucket/pointer indirection.

**Lookup helper:**

```cpp
std::optional<std::string_view> find(std::string_view name) const {
    for (auto& h : headers)
        if (ci_equal(h.name, name)) return h.value;
    return std::nullopt;
}

std::vector<std::string_view> find_all(std::string_view name) const; // for repeated fields
```

If profiling later shows lookups are hot at high header counts, add an
**auxiliary index** (`unordered_multimap<string, size_t>` mapping name →
vector index) on top — keep the vector as the source of truth for order,
don't replace it.

### 2.5 Limits — constructor parameters, not constants

This is the actual defense against memory-exhaustion DoS once this thing
faces untrusted input:

```cpp
struct Limits {
    size_t max_start_line_len = 8192;
    size_t max_header_count   = 100;
    size_t max_header_line_len = 8192;
    size_t max_total_header_bytes = 64 * 1024;
    size_t max_chunk_size_bytes = 8 * 1024 * 1024;
    size_t max_body_bytes = 10 * 1024 * 1024; // 0 or optional<> = unlimited, if you want that option
    static Limits defaults();
};
```

Every limit must be enforced **incrementally** (reject as soon as the limit
is exceeded) — not "buffer everything, then check," which defeats the
purpose.

### 2.6 Terminology: OWS, RWS, BWS

These whitespace tokens (RFC 9110 §5.6.3) show up throughout the grammar and
your parser needs to treat each one differently:

| Token | Meaning | Definition | Where it matters |
|---|---|---|---|
| **OWS** | Optional whitespace | zero or more `SP`/`HTAB` | Around header values: `field-line = field-name ":" OWS field-value OWS` — both sides are stripped, not semantic |
| **RWS** | Required whitespace | one or more `SP`/`HTAB` | Between tokens where whitespace is mandatory, e.g. separating the method/target/version in the request line |
| **BWS** | "Bad" whitespace | same production as OWS | Used in a couple of legacy grammar spots where whitespace is tolerated for historical compatibility even though conforming senders shouldn't generate it |

**The rule that matters most for Milestone 3:** there is **no OWS between
the field-name and the colon**. Whitespace there doesn't match the
`field-line` production at all — it's not "whitespace to trim," it's a
different, invalid construct that must be rejected outright. This is a
spec-mandated MUST-reject specifically because permissive handling here is
a known request-smuggling vector.

---

## 3. Milestones

### Milestone 0 — Scaffolding

- CMake project, C++20, warnings-as-errors (`-Wall -Wextra -Wpedantic` or
  MSVC equivalent).
- Test framework: doctest.
- `Http11RequestParser` class skeleton + `Limits` struct, compiling with
  empty method bodies.

**Done when:** project builds, an empty test suite runs green.

---

### Milestone 1 — Byte-level primitives

Build the low-level building blocks everything else depends on:

- Character classification tables for `tchar` (token characters, used in
  method names and header field names) and `vchar` (visible/printable
  characters used in header values), per RFC 9110 §5.6.2.
- `OWS`/`RWS` skipping helpers.
- A CRLF scanner that correctly reports "need more data" when a `CRLF` is
  split across the current buffer boundary — it must **not** misinterpret
  a lone trailing `CR` as "no CRLF found, error," nor as "found," until the
  next byte confirms it.
- Bonus safety check worth building now: reject a lone `LF` not preceded by
  `CR` if you want to be strict (RFC 9112 allows recipients to accept bare
  `LF` as a line terminator for robustness, but you should make this an
  explicit, deliberate choice, not an accident).

**Test cases to cover:**

| Input | Expected |
|---|---|
| Full byte range 0x00–0xFF | Correct tchar/vchar classification per table |
| `"...\r\n"` fed whole | CRLF found |
| `"...\r"` then `"\n..."` fed separately | NeedMoreData, then CRLF found on second call |
| `"...\r"` then `"X..."` (not `\n`) | Error (or defined bare-CR policy) |

**Done when:** classification tests pass for the full byte range; split-CRLF
does not misparse in either call-order.

---

### Milestone 2 — Request-line parsing

Grammar shape: `METHOD SP request-target SP HTTP-version CRLF`.

Implementation notes:

- Method: sequence of `tchar`, case-sensitive (methods are case-sensitive
  per spec, unlike header names).
- Exactly one `SP` between fields — reject tabs, multiple spaces, or other
  whitespace variants here. This is a MUST-reject, not a "be liberal"
  opportunity: permissive whitespace handling in the request line is a
  documented smuggling vector when client and server/proxy disagree on
  where the line's structure actually is.
- `HTTP-version`: literal `HTTP/` followed by `DIGIT "." DIGIT`. Reject
  anything that doesn't match this exactly (`HTTP/1.1`, `HTTP/1.0`); decide
  explicitly whether to accept `HTTP/2` or `HTTP/3` on this parser (probably
  not — different wire format upstream of this point).
- Request-target: support `origin-form` (`/path?query`) at minimum. Decide
  and document your stance on `absolute-form` (full URI, used when talking
  to a proxy), `authority-form` (`CONNECT`), and `asterisk-form` (`OPTIONS *`).

**Test cases to cover:**

| Case | Expected |
|---|---|
| `GET /index.html HTTP/1.1\r\n` | Parses cleanly |
| `GET  /index.html HTTP/1.1\r\n` (double space) | Reject |
| `GET\t/index.html HTTP/1.1\r\n` (tab) | Reject |
| `GET /index.html HTTP/1.1` (no CRLF, buffer ends) | NeedMoreData |
| Line longer than `max_start_line_len` | Reject before buffering the whole thing |
| `GET /index.html HTTP/1.10\r\n` | Reject (not `DIGIT "." DIGIT`) |
| Fed one byte at a time vs. fed whole | Identical result |

**Done when:** table-driven valid/invalid tests pass; oversized lines are
rejected without unbounded buffering; byte-at-a-time feeding matches
whole-buffer feeding.

---

### Milestone 3 — Header-block parsing

Grammar shape (repeated): `field-name ":" OWS field-value OWS CRLF`, an
empty `CRLF` line terminates the block.

Implementation notes:

- **Reject any whitespace between the field-name and the colon** — see
  §2.6 above. This is the single most important rule in this milestone.
- **Reject obsolete line-folding** (a header value continuing on the next
  line, indented) outright. It's a MUST-reject in current RFCs and, like
  the colon-whitespace rule, directly smuggling-relevant.
- Field names: case-insensitive comparison, but store as received (don't
  normalize case in storage — you may want it for logging/debugging).
- Field values: trim `OWS` from both sides; the trimmed value is what's
  semantically meaningful.
- Duplicates: preserve all occurrences in the vector. Whether duplicates
  are *legal* is field-specific (e.g. multiple `Set-Cookie` is normal;
  multiple `Content-Length` with different values is not — that check
  belongs in Milestone 4, but you need duplicates preserved here to detect
  it there).
- Enforce `max_header_count`, `max_header_line_len`, and
  `max_total_header_bytes` incrementally as each field-line completes.

**Test cases to cover:**

| Case | Expected |
|---|---|
| `Host: example.com\r\n` | Parses, value `example.com` |
| `Host : example.com\r\n` (space before colon) | Reject |
| `Host:example.com\r\n` (no space after colon) | Parses (OWS is optional) |
| Folded header (continuation line starting with space) | Reject |
| Header line split across two `feed()` calls | NeedMoreData then Complete-for-that-field, identical to whole-buffer |
| More than `max_header_count` fields | Reject once limit crossed, not after buffering all |
| Empty line (`\r\n` alone) | Terminates header block, transitions to body-length determination |

**Done when:** colon-whitespace and obs-fold rejection tests pass; all
count/size limits are enforced incrementally.

---

### Milestone 4 — Body-length determination + Content-Length bodies

This is where most smuggling bugs historically live — implement the
precedence rules deliberately, in this order:

1. **No-body cases first**: responses to `HEAD`, and status codes
   `1xx`/`204`/`304` never have a body regardless of headers present (this
   rule is about *responses*; for a request-only parser, the analogous
   request-side case is simpler — a request body's presence is generally
   signaled by `Content-Length` or `Transfer-Encoding` being present at all).
2. **`Transfer-Encoding` takes precedence over `Content-Length`** when
   both would otherwise apply.
3. **`Content-Length`** if present alone: exact byte count, must be a valid
   non-negative integer.
4. **Close-delimited** (read until connection close) is a response-only,
   last-resort mechanism — not applicable to requests, since a client can't
   signal end-of-body via closing before the server has replied.

**Critical rejection rules** (these are the actual smuggling defenses):

- **Reject any message with both `Content-Length` and `Transfer-Encoding`
  present.** Don't try to reconcile them or pick a "winning" one — reject
  outright. This exact ambiguity is the classic CL.TE / TE.CL smuggling
  pattern between a front-end and back-end that disagree on which one
  wins.
- **Reject multiple `Content-Length` headers with differing values.**
  (Multiple identical values are sometimes tolerated by implementations,
  but decide and document your policy — rejecting all duplicates outright
  is the safer default.)
- **Reject a `Content-Length` value that isn't a clean non-negative
  integer** — no leading zeros ambiguity, no whitespace tricks, no signs.

**Test cases to cover:**

| Case | Expected |
|---|---|
| `Content-Length: 14` + 14 bytes body | Parses, body captured |
| `Content-Length: 14` + body split across 3 `feed()` calls | NeedMoreData ×2, Complete on the byte that completes it |
| `Content-Length: 5` and `Content-Length: 10` (differing) | Reject |
| `Content-Length: 5` and `Content-Length: 5` (identical, duplicated) | Document your policy either way |
| Both `Content-Length` and `Transfer-Encoding: chunked` present | Reject |
| `Content-Length: -1` / `Content-Length: 4x` | Reject |
| No `Content-Length`, no `Transfer-Encoding`, method with no defined body | Treated as zero-length body |

**Done when:** all no-body cases, the conflicting-header rejections, and
body-split-across-calls tests pass.

---

### Milestone 5 — Chunked transfer-encoding

Grammar shape (repeated): `chunk-size [chunk-ext] CRLF chunk-data CRLF`,
terminated by a zero-size chunk, optional trailer field-lines, final `CRLF`.

Implementation notes:

- `chunk-size` is hexadecimal — parse it as such, enforce
  `max_chunk_size_bytes` before allocating/copying the chunk data.
- **Chunk extensions** (`;name=value` after the size): decide your policy —
  parsing past them and ignoring semantically is reasonable for a v1; just
  don't let unbounded extension text bypass your line-length limits.
- **Trailers**: header-line syntax after the terminal zero chunk. Expose
  these **separately** from the main header set rather than silently
  merging them in — a trailer arriving after the body was already
  processed has different trust implications than a header seen up front
  (this matters a lot if a server ever makes a decision based on headers
  before the body is fully read).
- `Transfer-Encoding` can technically list multiple codings
  (`chunked` should always be last if present) — for a v1 it's reasonable
  to only support `Transfer-Encoding: chunked` alone and reject anything
  else as unsupported, documenting that as a known limitation rather than
  guessing at semantics for codings you haven't implemented.

**Test cases to cover:**

| Case | Expected |
|---|---|
| Single chunk + terminal zero-chunk, no trailers | Parses, body reassembled |
| Multiple chunks | Parses, body is concatenation in order |
| Zero-length body (`0\r\n\r\n` immediately) | Parses, empty body |
| Chunk-size line split mid-hex-digits across `feed()` calls | NeedMoreData then correct parse |
| Chunk data split mid-chunk across `feed()` calls | NeedMoreData then correct parse |
| Invalid hex in chunk-size | Reject |
| Chunk size exceeding `max_chunk_size_bytes` | Reject before buffering the chunk |
| Terminal chunk followed by trailer fields | Trailers captured separately from main headers |

**Done when:** all of the above pass, including both split points (chunk-
size line split, chunk-data split) independently.

---

### Milestone 6 — Public facade

- Wrap Milestones 1–5 behind the single `Http11RequestParser` class from
  §2.1.
- `reset()` — after a request completes, a keep-alive connection will send
  another one; `reset()` should restore parse state without reallocating
  the internal buffers each time (capacity retained, contents cleared).
- **Body delivery policy** — decide explicitly:
  - *Whole-buffer-on-complete*: simplest, fine for small bodies, `body()`
    returns a view once `ParseStatus::Complete`.
  - *Streaming callback*: `on_body_chunk(callback)` fired as body bytes
    arrive — matters once you're accepting uploads of non-trivial size,
    since whole-buffering defeats the point of the incremental design for
    large bodies.
  - A reasonable v1 default: whole-buffer, with a documented size cap via
    `Limits::max_body_bytes`; note streaming as a clear extension point.

**Test cases to cover:**

| Case | Expected |
|---|---|
| Real captured browser/curl request, fed whole | Correct parse |
| Same request, fed one byte at a time | Identical parse result |
| Same request, fed in randomized chunk sizes (property-test style) | Identical parse result across many random splits |
| Two requests back-to-back on one connection, `reset()` between | Both parse correctly, no state leakage between them |

**Done when:** a real request parses identically regardless of how its
bytes are split across `feed()` calls, and `reset()` correctly clears state
for reuse.

---

### Milestone 7 — Robustness pass (stretch, but high value)

- **Fuzz** the public `feed()` entry point with libFuzzer or AFL++, built
  under **ASan + UBSan**. This is where a parser like this earns trust.
- Build an explicit **smuggling-pattern test corpus** (see Appendix B) —
  don't rely on the fuzzer alone to find these; they're specific known
  patterns worth encoding as permanent regression tests.
- Verify limits actually **bound memory** under adversarial input — e.g.
  feed a request with no CRLF ever, in a tight loop, and confirm memory
  usage plateaus rather than growing unboundedly before the limit trips.
- Consider differential testing against another parser (e.g. Node's
  `http_parser`/`llhttp`, or Python's `http.client` internals) on a shared
  corpus of edge-case requests, to catch places where your interpretation
  of the spec silently diverges from a battle-tested implementation.

**Done when:** a multi-hour (or CI-time-budgeted) fuzz run under ASan/UBSan
turns up nothing; the smuggling-pattern corpus all reject as expected;
memory usage under adversarial slow-feed input is bounded and verified.

---

## 4. Definition of Done (final checklist)

- [ ] Parses every method and every request-target form you chose to support
- [ ] Rejects, without crashing, all MUST-reject cases: bad whitespace
      (including before the colon), obsolete line-folding, conflicting
      `Content-Length`/`Transfer-Encoding`, malformed HTTP-version, control
      characters in names/values, all oversized-field limits
- [ ] **Split-invariance holds**: for a corpus of valid requests, parsing
      result is byte-for-byte identical no matter how the input is split
      across `feed()` calls — automate this (take each corpus request, try
      many random split points, assert identical output) rather than
      testing it ad hoc
- [ ] No unbounded buffering under adversarial input (verified, not assumed)
- [ ] Clean under ASan/UBSan across a fuzz run
- [ ] README documents exactly what subset of HTTP/1.1 is supported, and
      what's explicitly out of scope (100-continue behavior, pipelining
      semantics, proxy-form targets, multiple Transfer-Encoding codings,
      etc.) — being explicit about gaps is as valuable as the feature list

---

## Appendix A — Grammar quick reference (paraphrased from RFC 9110/9112)

```
request-line   = method SP request-target SP HTTP-version CRLF
field-line     = field-name ":" OWS field-value OWS
HTTP-version   = "HTTP/" DIGIT "." DIGIT
OWS            = *( SP / HTAB )              ; optional whitespace
RWS            = 1*( SP / HTAB )             ; required whitespace
chunk          = chunk-size [ chunk-ext ] CRLF chunk-data CRLF
last-chunk     = 1*"0" [ chunk-ext ] CRLF
```

(Paraphrased structure for implementation reference — consult RFC 9110 §5–6
and RFC 9112 §3–7 directly for the authoritative, complete grammar before
implementing each milestone.)

## Appendix B — Known smuggling patterns worth a permanent regression test

- `Content-Length` **and** `Transfer-Encoding` both present (CL.TE / TE.CL)
- Duplicate `Transfer-Encoding` headers, or `Transfer-Encoding` with an
  unexpected/obfuscated value (e.g. extra whitespace, mixed case tricks,
  a value like `chunked` hidden among other tokens)
- Duplicate `Content-Length` headers with differing values
- Obsolete line-folding used to hide a second header line from one parser
  while another parser sees it as a continuation
- Whitespace before the colon in a field-line
- A bare `LF` where `CRLF` is expected, if your policy is to reject it
- Chunk-size line with trailing content designed to look like the start of
  a second request (e.g. embedded CRLFCRLF inside chunk extensions)

## Appendix B — Server integration notes (for later)

Keep the parser **I/O-free**. Integration should look like:

```
non-blocking read() → parser.feed(buf, n) → switch on ParseStatus:
    NeedMoreData → keep reading
    Complete     → hand request_line()/headers()/body() to request handler,
                    then parser.reset() for the next request on this connection
    Error        → send appropriate 4xx and close the connection
```

That separation — sockets and event loop entirely outside the parser — is
what makes it independently testable and fuzzable now, and is the reason
this exercise won't require a rewrite when it becomes part of a server.
