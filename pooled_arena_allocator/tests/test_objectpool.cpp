// AI-Generated
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "../arena.h"
#include "../objectpool.h"
#include "doctest.h"
#include <cstdint>

namespace {
struct Point {
  int x;
  int y;
};

// A type large enough to ensure it comfortably holds a void* for the free list
struct Dummy {
  uint64_t data[4];
};
} // namespace

TEST_CASE("ObjectPool Basic Allocation and Functional Verification") {
  auto arena_opt = Arena::create();
  REQUIRE(arena_opt.has_value());
  Arena arena = std::move(*arena_opt);

  ObjectPool<Point> pool(arena);

  SUBCASE("Single allocation returns a valid writable pointer") {
    Point *p = pool.alloc();
    REQUIRE(p != nullptr);

    p->x = 10;
    p->y = 20;
    CHECK(p->x == 10);
    CHECK(p->y == 20);
  }

  SUBCASE("Successive allocations return unique, non-overlapping storage") {
    Point *p1 = pool.alloc();
    Point *p2 = pool.alloc();

    REQUIRE(p1 != nullptr);
    REQUIRE(p2 != nullptr);
    CHECK(p1 != p2);
  }
}

TEST_CASE("ObjectPool Intrusive Free List & Recycling Semantics") {
  auto arena_opt = Arena::create();
  Arena arena = std::move(*arena_opt);
  ObjectPool<Dummy> pool(arena);

  SUBCASE("Freeing a null pointer handles gracefully without side effects") {
    pool.free(nullptr);
    Dummy *p = pool.alloc();
    CHECK(p != nullptr);
  }

  SUBCASE("Freeing an allocated object recycles it for the next allocation") {
    Dummy *p1 = pool.alloc();
    REQUIRE(p1 != nullptr);

    pool.free(p1);

    // The very next allocation should pull directly from the free list head
    Dummy *p2 = pool.alloc();
    CHECK(p1 == p2);
  }

  SUBCASE("LIFO ordering: Multiple freed objects are recycled in Last-In, "
          "First-Out sequence") {
    Dummy *p1 = pool.alloc();
    Dummy *p2 = pool.alloc();
    Dummy *p3 = pool.alloc();
    REQUIRE(p1 != nullptr);
    REQUIRE(p2 != nullptr);
    REQUIRE(p3 != nullptr);

    // Free in order: p1, then p2, then p3
    pool.free(p1);
    pool.free(p2);
    pool.free(p3);

    // Head of free list should point to the last item pushed (p3)
    Dummy *r1 = pool.alloc();
    CHECK(r1 == p3);

    Dummy *r2 = pool.alloc();
    CHECK(r2 == p2);

    Dummy *r3 = pool.alloc();
    CHECK(r3 == p1);

    // Next allocation has to fall back to the Arena
    Dummy *r4 = pool.alloc();
    CHECK(r4 != p1);
    CHECK(r4 != p2);
    CHECK(r4 != p3);
  }
}

TEST_CASE("ObjectPool Lifecycle Reset Semantics") {
  auto arena_opt = Arena::create();
  Arena arena = std::move(*arena_opt);
  ObjectPool<Point> pool(arena);

  SUBCASE("Reset clears both the underlying arena and the internal free list") {
    Point *p1 = pool.alloc();
    REQUIRE(p1 != nullptr);

    pool.free(p1);

    // Reset must clear the free list head and rewind the Arena
    pool.reset();

    Point *p2 = pool.alloc();
    REQUIRE(p2 != nullptr);

    // Because the free list was cleared, it fell back to the arena.
    // Because the arena was reset, the arena's cur pointer rewound,
    // overlaying the allocation at the exact same base address point.
    CHECK(p1 == p2);
  }
}
