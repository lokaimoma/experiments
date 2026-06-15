> 🤖 **AI-Generated** — This document was produced after a full static analysis of the codebase (arena.h, arena.cpp, objectpool.h, and their test suites). The AI traced every code path, validated alignment logic, block traversal, free-list mechanics, and move semantics before synthesizing this description. Minor imprecisions may exist; the source files remain the source of truth.

# Pooled Arena Allocator

A two-tier C++20 memory allocation library combining a **region-based (arena) allocator** with an **intrusive free-list object pool** built on top of it.

---

## Architecture

```
┌────────────────────────────────────────────────────────────┐
│                     ObjectPool<T>                          │
│  ┌──────────────────────────────────────────────────────┐  │
│  │  Intrusive free list (LIFO recycling of freed T's)   │  │
│  └──────────────────────────────────────────────────────┘  │
│                          │ alloc() / free()                │
│                          ▼                                 │
│  ┌──────────────────────────────────────────────────────┐  │
│  │                     Arena                            │  │
│  │  ┌──────────┐  ┌──────────┐  ┌──────────┐            │  │
│  │  │ Block 1  │→│ Block 2  │→│ Block N  │  (linked)    │  │
│  │  │ (40 MB)  │  │ (40 MB)  │  │ (varies) │            │  │
│  │  └──────────┘  └──────────┘  └──────────┘            │  │
│  └──────────────────────────────────────────────────────┘  │
└────────────────────────────────────────────────────────────┘
```

### Arena (`arena.h`, `arena.cpp`)

The Arena is a **linear / bump allocator** — it allocates memory by advancing a pointer through pre-obtained blocks. Never frees individual allocations; instead the entire arena is **reset** or **destroyed** as a whole.

#### Block List

Memory is carved from a singly-linked list of `MemoryBlock` nodes. Each block tracks three pointers:

```
Block:  [start]────────────────────[cur]──────────[end]
           │                          │               │
        base addr               next free pos     capacity limit
```

- `start` — address returned by `malloc`
- `cur` — current bump-pointer (next allocation starts here)
- `end` — one past the last usable byte

On creation, the Arena allocates one default-sized block (40 megabytes). When it runs out of space, `request_new_block()` is called.

#### Allocation Path (`arena.h:alloc<T>()`)

1. **Bounds check** — rejects zero-count and overflow of `count * sizeof(T)`.
2. **Alignment** — uses `std::align(alignof(T), ...)` to advance `cur` to the next properly aligned address within the current block.
3. **Fast path** — if the aligned allocation fits in the current block, `cur` is bumped and the aligned pointer is returned.
4. **Block reuse** — before allocating a new block, the Arena scans the existing blocklist for a block large enough to satisfy the request. If found, that block is spliced to the front of the active list and becomes the new active block.
5. **Growth** — if no existing block is large enough, a new block is allocated via `malloc`. Its size is the larger of `DEFAULT_BLOCK_SIZE` (40 megabytes) and the requested capacity, so oversized allocations get dedicated blocks without wasting the default pool.
6. **Recursive retry** — after obtaining a new block, `alloc<T>()` retries the allocation on the fresh block.

#### Reset

`reset()` rewinds `cur` back to `start` on **every** block in the list and sets `active_block` to the head. No memory is returned to the OS; subsequent allocations will reuse all existing storage. This makes reset an O(n) operation over the number of blocks.

#### Move Semantics

Arena is move-only (deleted copy constructor/assignment). A moved-from arena is left in a null state (nullptr head/active).

#### Factory: `Arena::create()`

The constructor is private; arenas are created through the static `create()` method which returns `std::optional<Arena>`. This guarantees every Arena has at least one valid block before the user receives it.

### ObjectPool (`objectpool.h`)

The ObjectPool wraps an `Arena&` and adds **individual-object recycling** via an intrusive free list.

#### Intrusive Free List

When a user calls `pool.free(ptr)`, the pool writes the current free-list head pointer into the first `sizeof(void*)` bytes of the object's memory (reinterpreting the object itself as a linked-list node). On the next `alloc()`, the pool pops from this list first, only falling back to the Arena when the free list is empty.

```
Free list:

  head_of_free_list ─→ [ freed C ] ─→ [ freed B ] ─→ [ freed A ] ─→ nullptr
                         (memory of     (memory of      (memory of
                          former C)      former B)       former A)
```

The free list is **LIFO** (stack order): the most recently freed object is the first to be recycled.

#### Reset

`pool.reset()` calls `arena.reset()` to rewind all arena blocks, then clears the free-list head to `nullptr`. After a reset, the free list is empty and the arena starts allocating from the beginning again — so a post-reset `alloc()` returns the same address as the very first pre-reset allocation.

#### Requirements

Because the free list pointer is stored directly in freed memory, every type `T` managed by ObjectPool must be at least `sizeof(void*)` bytes in size (or have padding that accommodates it). No separate free-list node allocation is needed.

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
| Allocation strategy | Linear bump | Fastest possible — single pointer bump + alignment |
| Deallocation | Bulk (reset / destructor) | No per-object free overhead; ideal for phase-based workloads |
| Block growth | Geometric-ish (fixed 40 megabytes + oversized carve-outs) | Reduces number of malloc calls; oversized allocations don't fragment the default pool |
| Block reuse on alloc | Scans existing blocks | Avoids malloc for workloads that cycle through multiple blocks |
| Object recycling | Intrusive free list (LIFO) | Zero overhead for the free-list data structure; cache-friendly for stack-ordered lifetimes |
| Safety | Move-only, factory constructor, ASan/UBSan in tests | Prevents accidental copies; factory ensures valid initial state |
