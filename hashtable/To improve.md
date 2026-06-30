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
**Status:** No longer a concern

This was coupled to issue #4 — the risk was the secondary growing unchecked during
a stalled rehash. Now that every operation (including `get()`) advances the rehash
by 128 buckets, the rehashing window is too short for the secondary to meaningfully
accumulate entries. The load factor check still doesn't run during rehashing, but
there's no realistic scenario where it would matter.

---

## 6. Rehash work is bucket-counted, not entry-counted

**File:** `hmap.h:193-199`
**Status:** Fixed

The loop now counts entries moved (`i` increments only when `a` is non-null) and
stops after `k_rehash_work` entries have been processed. For sparse tables where
most buckets are empty, this means more buckets get scanned per call until 128
actual entries are found — but each call does a meaningful amount of work. Worst-case
progress is still bounded (at most ceil(cap / 128) calls) since `i < primary_cap`
catches the case where we scan the entire table.

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

**Status:** Fixed

There's no way to query the number of live entries in the map. Useful for both
callers and internal assertions in tests.

## 10. `Table::operator[]` does a bounds check via exception

**File:** `table.cpp:66-71`
**Status:** Not relevant

`operator[]` throws `std::out_of_range` on invalid index, but it's called in
`perform_rehash()` where the index is always guarded by `rehash_idx >= primary_cap`.
The exception path is dead code in practice. Consider using an unchecked accessor
internally (`TNode *&tnode_at(size_t idx)`) for the rehash hot path, and keep the
checked version for public use.
