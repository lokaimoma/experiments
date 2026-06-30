# Bugs Found & Fixed

## 1. `TNode *&next` reference truncates collision chains during rehashing

**File:** `hmap.h:193`  
**Status:** Fixed

```cpp
// BEFORE (bug):
TNode *&next{a->next};   // reference to a->next
a->next = nullptr;       // ALSO sets next = nullptr (same memory)
t.insert(a);
a = next;                // always nullptr ← chain truncated after 1st entry

// AFTER (fix):
TNode *next{a->next};    // copy, not reference
a->next = nullptr;
t.insert(a);
a = next;
```

`TNode *&next{a->next}` creates a **reference** to `a->next`. When `a->next = nullptr`
executes, `next` becomes `nullptr` too (it references the same memory location). The
subsequent `a = next` always sets the bucket head to `nullptr` after the first entry,
dropping every subsequent node in any bucket with hash collisions.

**Impact:** Any bucket with 2+ entries (collisions) loses all entries after the first
during rehashing. With 769 entries in 1024 buckets, the birthday paradox guarantees
many collisions — entries are silently lost.

---

## 2. `Table::lookup` not const-qualified

**File:** `table.h:34`, `table.cpp:61`  
**Status:** Fixed

`lookup()` only reads member variables (`cap_mask`, `tnodes`) and never modifies them,
but it was declared without `const`. This prevented `HMap::get()` (which is `const`)
from calling it on the const member `htab_primary`.

The same issue existed on the `htab_secondary` path inside `get()` — `Table &t` couldn't
bind to the `const Table` returned by `htab_secondary.value()` in a const context.
Fixed to `const Table &t` at `hmap.h:162`.

---

# Remaining Issues

## 3. `htab_primary.size` is not decremented during rehashing

**File:** `hmap.h:40,108,114,150`  
**Status:** Fixed

`HMap` now tracks `live_count` — the total number of live entries across both primary
and secondary tables. It is incremented in `add()` and decremented in `remove()`.
The load-factor check in `add()` uses `live_count` instead of `htab_primary.get_size()`,
so the trigger is accurate regardless of rehashing state. A `size()` accessor is also
exposed on `HMap`.

---

## 4. Rehashing stalls under read-heavy traffic

**File:** `hmap.h:37-40,60,155-179`  
**Status:** Fixed

`htab_primary`, `htab_secondary`, `rehash_idx`, and `live_count` are marked `mutable`,
and `perform_rehash()` is marked `const`. `get()` now calls `perform_rehash()` on every
lookup while remaining `const`. This ensures rehashing progresses regardless of which
operations are called, without sacrificing const-correctness in the public API.

---

## 5. Load factor check only evaluates when not rehashing, and only on primary size

**File:** `hmap.h:107-116`  
**Status:** Not fixed

The load-factor check on line 112:
```cpp
if ((size * 4 > cap * 3)) { // 75% max load factor
```

- Only runs in the `!is_rehashing()` branch (line 107).
- Reads `htab_primary.get_size()`, which is inflated during rehashing (issue #3).

**Consequence:** If rehashing stalls (issue #4), new entries keep piling into
`htab_secondary` at the doubled capacity (e.g. 2048). To exceed 75% of 2048 you'd
need ~1536 total entries, which takes ~768 new inserts during the stalled rehashing
window. Each new `add()` during rehashing calls `perform_rehash()` once, which moves
128 buckets — so normally the rehash completes long before this is a problem.

But in a pathological scenario where most `perform_rehash()` calls process empty
buckets (e.g. map is sparse) and `get()` is the dominant operation, rehashing barely
advances while new entries accumulate in the secondary. The secondary could exceed
its intended load factor, degrading lookup performance with longer chains, and never
trigger a new resize because the trigger only checks primary.

---

## 6. Rehash work is bucket-counted, not entry-counted

**File:** `hmap.h:180-199`  
**Status:** Not fixed

`k_rehash_work = 128` processes 128 **buckets** per call, regardless of how many
entries are actually in those buckets. If most buckets are empty (sparse map), a call
to `perform_rehash()` may do almost no useful work — the `while (a)` loop immediately
exits for each empty bucket.

**Consequence:** Rehashing progresses slowly for sparse tables. After N calls to
`perform_rehash()`, N * 128 buckets are "processed" but only a handful of entries
were actually moved. The rehash completion check (`rehash_idx >= primary_cap`)
signals completion once all buckets are scanned, which happens promptly — but the
real work (moving entries) happens gradually.

This is more of a performance characteristic than a correctness bug. The bucket-based
approach is simple and guarantees eventual completion in a bounded number of calls
(ceil(cap / 128)).

**Potential improvement:** Track entries-moved instead of buckets-processed.
Process buckets until `k_rehash_work` entries have been moved, rather than until
128 buckets have been visited.

---

# Design Improvements (Not Bugs)

## 7. `container_of` macro uses GCC `typeof` extension

**File:** `hmap.h:14-18`

The `container_of` macro uses `typeof`, which is a GNU extension. This won't compile
with MSVC. If portability matters, use a standard-compatible approach (e.g. relying
on a template function with `std::declval` or using `decltype` with a helper).

## 8. Test coverage for string keys during rehashing

**File:** `tests/test_hmap.cpp`

The rehashing tests only use `int` keys. The `internal_key()` path for string types
(arena-backed copy) is not exercised during rehashing. Adding a test case with string
keys during rehashing would cover arena interaction + rehashing.

## 9. No `size()` method on `HMap`

There's no way to query the number of live entries in the map. Useful for both
callers and internal assertions in tests.

## 10. `Table::operator[]` does a bounds check via exception

**File:** `table.cpp:66-71`

`operator[]` throws `std::out_of_range` on invalid index, but it's called in
`perform_rehash()` where the index is always guarded by `rehash_idx >= primary_cap`.
The exception path is dead code in practice. Consider using an unchecked accessor
internally (`TNode *&tnode_at(size_t idx)`) for the rehash hot path, and keep the
checked version for public use.
