> 🤖 **AI-Generated** — This document was produced after a full static analysis of the codebase (table.h, table.cpp, hmap.h, hmaphasher.h, and their test suites). The AI traced every code path, validated hashing, collision handling, incremental rehashing, mutable rehash state, and move semantics before synthesizing this description. Minor imprecisions may exist; the source files remain the source of truth.

# HashTable

**Inspired by the [Build Your Own Redis](https://build-your-own.org/redis/) tutorial** — the incremental rehashing design, power-of-two capacity with bit mask indexing, and the high-level map wrapping a low-level table are concepts adapted from that guide. This implementation extends the original ideas with C++20 templates, arena-backed string storage, an intrusive free-list object pool, and additional hasher specializations.

A C++20 hash map library combining an **FNV-1a hasher**, a **power-of-two open-addressing hash table with separate chaining**, and a high-level **map with arena-backed storage and incremental rehashing**.

---

## Architecture

```
┌──────────────────────────────────────────────────────────────────┐
│                          HMap<K, V>                              │
│  ┌────────────────────────────────────────────────────────────┐  │
│  │  Add / Remove / Get (key lookup with collision resolution) │  │
│  │  Incremental rehashing (mutable state)                     │  │
│  │  Live count tracking (size())                              │  │
│  └────────────────────────────────────────────────────────────┘  │
│                          │ delegates to                          │
│                          ▼                                       │
│  ┌────────────────────────────────────────────────────────────┐  │
│  │                          Table                             │  │
│  │  ┌──────┐ ┌──────┐ ┌──────┐ ┌──────┐                       │  │
│  │  │bucket│→│bucket│→│bucket│→│bucket│→ ... (separate chain) │  │
│  │  │  0   │ │  1   │ │  2   │ │  3   │                       │  │
│  │  └──┬───┘ └──┬───┘ └──┬───┘ └──┬───┘                       │  │
│  │     ▼        ▼        ▼        ▼                           │  │
│  │   TNode → TNode → ...  (linked list per bucket)            │  │
│  └────────────────────────────────────────────────────────────┘  │
│                          │ backed by                             │
│                          ▼                                       │
│  ┌────────────────────────────────────────────────────────────┐  │
│  │                     Arena + ObjectPool                     │  │
│  │  (region allocator + intrusive free list for MapEntry)     │  │
│  └────────────────────────────────────────────────────────────┘  │
└──────────────────────────────────────────────────────────────────┘
```

### Hasher (`hmaphasher.h`)

`HMapHasher<T>` implements **FNV-1a** hashing with explicit specializations for:

| Type | Approach |
|---|---|
| `int` | Byte-wise FNV-1a on the 4-byte representation |
| `float` | Normalizes `-0.0f` to `0.0f`, then hashes via `std::bit_cast` + FNV-1a |
| `double` | Normalizes `-0.0` to `0.0`, then hashes via `std::bit_cast` + FNV-1a |
| `std::string_view` | Byte-wise FNV-1a over the string data |
| `std::string` | Forwards to `std::string_view` hasher |
| `const char*` | Forwards to `std::string_view` hasher (implicit conversion) |

The primary template routes all string-like types through `std::string_view` to avoid code duplication.

### Table (`table.h`, `table.cpp`)

A low-level **open-addressing hash table** using **separate chaining**:

- **Power-of-two capacity** — `cap` is always a power of 2; index = `hcode & (cap - 1)` instead of modulo.
- **TNode** — a singly-linked list node with a 32-bit hash code and a `next` pointer.
- **Insert** — prepends the node to the chain at `tnodes[index]`; increments `size`.
- **Detach** — finds a node matching both hcode and a user-supplied equality predicate, splices it out of its chain; decrements `size`.
- **Lookup** — walks the chain at the index; returns the first match (uses `const`).
- **Move semantics** — move constructor and assignment transfer ownership of the arena and bucket array; zeroes out the source.
- **Bounds-checked indexing** — `operator[]` throws `std::out_of_range` on out-of-bounds access (used by `perform_rehash()` where the guard is always evaluated first).

### HMap (`hmap.h`)

The high-level map built on top of `Table`:

#### Key Storage

Keys of type `std::string`, `std::string_view`, or `const char*` are copied into an **arena allocator** (owned by the map) and stored internally as `std::string_view`. This makes key storage stable regardless of the original string's lifetime.

#### Arena + ObjectPool

`MapEntry` objects (key, value, TNode) are allocated from an `ObjectPool` backed by the arena. The pool uses an **intrusive free list** — freed entries are recycled LIFO by storing the free-list pointer directly in the freed object's memory.

#### Incremental Rehashing

When the load factor exceeds 75%, the map creates a secondary table at double capacity and incrementally moves entries:

```
Before rehash:              During rehash:              After rehash:
┌──────────────┐            ┌──────────────┐            ┌──────────────┐
│  htab_primary│            │  htab_primary│            │  htab_primary│
│  (1024 cap)  │            │  (1024 cap)  │     ──→    │  (2048 cap)  │
│  768 entries │            │  ~640 entries│            │  768+ entries│
└──────────────┘            │  rehash_idx→ │            └──────────────┘
                            └──────────────┘
                            ┌──────────────┐
                            │htab_secondary│
                            │  (2048 cap)  │
                            │  ~128 entries│
                            └──────────────┘
```

Key design points:

- **Every operation advances rehashing** — `add()`, `remove()`, and even `get()` call `perform_rehash()`, so reads don't stall the process. The rehashing state (`htab_primary`, `htab_secondary`, `rehash_idx`, `live_count`) is `mutable`, allowing `get()` to remain `const` while advancing rehashing.
- **Entry-counted work** — Each call processes buckets until `k_rehash_work` (128) nonempty buckets have been found, not just 128 buckets regardless of content. This ensures meaningful progress even on sparse tables.
- **Live count** — `live_count` tracks the total number of entries across both tables, replacing reliance on `htab_primary.get_size()` (which is inflated during rehashing).

---

## Building & Testing

```bash
cmake -B build && cmake --build build
ctest --test-dir build -V
```

Tests use the [doctest](https://github.com/doctest/doctest) framework (fetched automatically by CMake) and are compiled with AddressSanitizer and UndefinedBehaviorSanitizer enabled.

---

## Key Trade-Offs

| Property | Choice | Rationale |
|---|---|---|
| Hash function | FNV-1a (non-cryptographic) | Simple, fast, good avalanche for non-adversarial keys |
| Collision resolution | Separate chaining | Simple, predictable performance; no open-addressing clustering |
| Table capacity | Power of 2 | Fast index via bit mask instead of modulo; only works with good hash diffusion |
| Rehashing trigger | 75% load factor | Balances memory usage vs collision probability |
| Rehashing strategy | Incremental (entry-counted) | Avoids latency spikes of bulk rehash; work is bounded per call |
| Rehashing advancement | Every operation (incl. `get()`) | Prevents stalls under read-heavy traffic |
| Key storage | Arena-backed copy for strings | Stable keys regardless of caller lifetime; no heap fragmentation |
| Entry recycling | Intrusive free list (LIFO) | Zero-overhead data structure; cache-friendly for stack-ordered lifetimes |
| `const`-correctness | `mutable` rehash state | `get()` stays `const`; rehashing is an invisible implementation detail |
| Safety | Move-only types, ASan/UBSan in tests | Prevents accidental copies; catches UB during development |
