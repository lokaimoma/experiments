# Chunked Transfer-Encoding Parsing

## Overview

Chunked encoding (RFC 7230 §4.1) allows a sender to stream a body without knowing its total size in advance. The body is split into chunks, each prefixed by its size in hex, and terminated by a zero-length chunk followed by optional trailers.

```
chunked-body = *chunk
               last-chunk
               trailer-section
               CRLF

chunk        = chunk-size [chunk-ext] CRLF
               chunk-data CRLF
chunk-size   = 1*HEXDIG
last-chunk   = 1*("0") [chunk-ext] CRLF

chunk-ext    = *( ";" token [ "=" ( token / quoted-string ) ] )

trailer-section = *( header-field CRLF )  -- empty line terminates
```

## Problem: Partial Reads

`read()` returns whatever is available — you may get half a chunk-size line, a chunk-size split across two calls, or multiple complete chunks in one read. The parser must maintain state between `decode()` calls.

## State Machine

Add these fields to `HttpRequest`:

```cpp
enum class ChunkedSubStage {
    size,      // reading chunk-size hex digits
    data,      // reading chunk-data bytes
    crlf,      // consuming \r\n after chunk-data
    trailers,  // parsing trailer headers
    done,      // body complete
};

ChunkedSubStage chunk_sub_stage{ChunkedSubStage::size};
size_t chunk_size_remain{0};  // bytes remaining in current chunk
std::vector<uint8_t> chunk_buf{};  // optional: temporary accumulator
```

The flow through `read_body` when `is_body_chunked`:

```
                ┌──────────────────────────────────────┐
                │                                      │
                ▼                                      │
 ┌──────┐  found CRLF  ┌──────┐  rem == 0  ┌──────┐   │
 │ size │──────────────►│ data │───────────►│ crlf │───┤
 └──────┘               └──────┘            └──────┘   │
    ▲ Found "0" \r\n                         found CRLF │
    │                                                 │
    ▼                                                 │
 ┌──────────┐  found empty line  ┌───────┐             │
 │ trailers │───────────────────►│ done  │──► end      │
 └──────────┘                    └───────┘
```

### 1. `size` sub-stage

Accumulate hex digits until `\r\n`. Parse the hex value into `chunk_size_remain`.

- **`chunk_size_remain == 0`** → last-chunk. Transition to `trailers`.
- **`chunk_size_remain > 0`** → Transition to `data`.

**Edge cases:**
- Hex digits may span multiple `decode()` calls — buffer incomplete lines.
- `chunk-ext` (e.g. `4;foo=bar`) must be stripped before parsing hex. Ignore everything after `;` on the size line.

### 2. `data` sub-stage

Copy raw bytes into `conn.req.body`, decrementing `chunk_size_remain`.

```
to_copy = min(buf.size(), chunk_size_remain)
body.insert(body.end(), buf.begin(), buf.begin() + to_copy)
chunk_size_remain -= to_copy
```

When `chunk_size_remain` reaches 0 → transition to `crlf`.

**Edge cases:**
- A single read may contain the end of one chunk, the CRLF, and the next chunk's size line. The data sub-stage only consumes `chunk_size_remain` bytes; remaining bytes must be fed to `crlf` (or back to `size`).

### 3. `crlf` sub-stage

Consume exactly the `\r\n` after chunk data. Expect exactly 2 bytes, but a single read might give only `\r` with `\n` in the next call. Buffer partial CRLFs.

After consuming `\r\n` → transition back to `size`.

### 4. `trailers` sub-stage

Same logic as the initial header parsing: read lines, stop at empty `\r\n`. Accumulate in a separate trailer map (or append to `headers`).

After empty `\r\n` → transition to `done`.

### 5. `done`

Set `conn.req.stage = RequestParsingStage::end`.

## Implementation Sketch

```cpp
void read_chunked_body(HttpConnection &conn, std::span<const uint8_t> buf) {
    auto &req{conn.req};
    size_t consumed{0};

    while (consumed < buf.size()) {
        auto span = buf.subspan(consumed);

        switch (req.chunk_sub_stage) {
        case ChunkedSubStage::size:
            // Find \r\n in span, accumulate hex, parse chunk_size_remain
            // On 0 -> trailers, else -> data
            break;

        case ChunkedSubStage::data:
            // Copy min(span.size(), chunk_size_remain) bytes to body
            // When exhausted -> crlf
            break;

        case ChunkedSubStage::crlf:
            // Consume \r\n (may be partial)
            // When complete -> size
            break;

        case ChunkedSubStage::trailers:
            // Parse header lines until empty \r\n
            // Then -> done
            break;

        case ChunkedSubStage::done:
            req.stage = RequestParsingStage::end;
            return;
        }
    }
}
```

The loop allows a single `decode()` call to process multiple chunks (e.g., `"4\r\nwiki\r\n5\r\npedia\r\n0\r\n\r\n"` in one read).

## Caveats

| Concern | Implication |
|---------|-------------|
| **chunk-ext** | Must strip extensions on size lines (`4;foo=bar`) before hex parse |
| **chunk-ext with trailing headers** | Some chunk-ext values are structural; can be ignored initially |
| **Trailers** | Trailers are optional headers — store in a separate map or merge into `headers` |
| **Content-Encoding** | Decompression (e.g., gzip) must happen *after* dechunking, on the reassembled body |
| **Max chunk size** | No RFC limit; a single chunk could exceed `MAX_BODY_LEN` — enforce total limit across all chunks |
| **Partial CRLF** | A CRLF may split as `\r` in one read, `\n` in the next — buffer the lone `\r` |
| **Connection close** | If the connection closes mid-chunk, the body is truncated — detectable via `read()` returning 0 |
