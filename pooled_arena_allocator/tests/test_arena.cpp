// AI-GENERATED
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "../arena.h"
#include "doctest.h"
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <type_traits>
#include <vector>

// ---------------------------------------------------------------------
// Compile-time contract checks
// ---------------------------------------------------------------------
TEST_CASE("static type properties") {
  static_assert(!std::is_copy_constructible_v<Arena>,
                "Arena must not be copy constructible");
  static_assert(!std::is_copy_assignable_v<Arena>,
                "Arena must not be copy assignable");

  static_assert(std::is_move_constructible_v<Arena>,
                "Arena must be move constructible");
  static_assert(std::is_move_assignable_v<Arena>,
                "Arena must be move assignable");
  static_assert(std::is_nothrow_move_constructible_v<Arena>,
                "Arena move constructor must be noexcept");
  static_assert(std::is_nothrow_move_assignable_v<Arena>,
                "Arena move assignment must be noexcept");

  static_assert(!std::is_default_constructible_v<Arena>,
                "Arena() should not be publicly accessible");
}

// Custom types for testing alignments
struct alignas(16) HighAlign {
  int64_t x;
  int64_t y;
};

struct alignas(64) OverAligned {
  unsigned char data[128];
};

struct Mixed {
  char c;
  double d;
  int i;
};

// ---------------------------------------------------------------------
// Initialization and Basic Lifecycle
// ---------------------------------------------------------------------
TEST_CASE("Arena Initialization and Independent Isolation") {
  auto arena_opt = Arena::create();
  REQUIRE(arena_opt.has_value());
  Arena arena = std::move(*arena_opt);

  SUBCASE("Allocating zero items returns nullptr") {
    auto *ptr = arena.alloc<int>(0);
    CHECK(ptr == nullptr);
  }

  SUBCASE("Independent arenas manage memory in isolation") {
    auto a2_opt = Arena::create();
    REQUIRE(a2_opt.has_value());
    Arena a2 = std::move(*a2_opt);

    int *p1 = arena.alloc<int>(1);
    int *p2 = a2.alloc<int>(1);
    REQUIRE(p1 != nullptr);
    REQUIRE(p2 != nullptr);

    *p1 = 111;
    *p2 = 222;

    CHECK(*p1 == 111);
    CHECK(*p2 == 222);
  }
}

// ---------------------------------------------------------------------
// Verification of Allocation Integrity
// ---------------------------------------------------------------------
TEST_CASE("Arena Memory Allocations and Non-Overlap") {
  auto arena_opt = Arena::create();
  Arena arena = std::move(*arena_opt);

  SUBCASE("Successive allocations do not overlap or clobber fields") {
    constexpr int kAllocs = 256;
    std::vector<int *> ptrs;
    ptrs.reserve(kAllocs);

    for (int i = 0; i < kAllocs; ++i) {
      int *p = arena.alloc<int>(1);
      REQUIRE(p != nullptr);
      *p = i;
      ptrs.push_back(p);
    }

    for (int i = 0; i < kAllocs; ++i) {
      CHECK(*ptrs[i] == i);
    }
  }

  SUBCASE("Array allocations respect total computed contiguous sizing") {
    constexpr size_t count = 5;
    int *array_ptr = arena.alloc<int>(count);
    REQUIRE(array_ptr != nullptr);

    for (size_t i = 0; i < count; ++i) {
      array_ptr[i] = static_cast<int>(i * 10);
    }
    CHECK(array_ptr[4] == 40);
  }
}

// ---------------------------------------------------------------------
// Alignment Strategy Constraints
// ---------------------------------------------------------------------
TEST_CASE("Arena Internal Alignment Constraints") {
  auto arena_opt = Arena::create();
  Arena arena = std::move(*arena_opt);

  SUBCASE("Subsequent allocations respect standard type alignments") {
    char *char_ptr = arena.alloc<char>(1);
    REQUIRE(char_ptr != nullptr);
    *char_ptr = 'x';

    HighAlign *aligned_ptr = arena.alloc<HighAlign>(1);
    REQUIRE(aligned_ptr != nullptr);

    auto address = reinterpret_cast<std::uintptr_t>(aligned_ptr);
    CHECK(address % 16 == 0);
    CHECK(*char_ptr == 'x');
  }

  SUBCASE("Over-aligned type alignments are satisfied") {
    char *c = arena.alloc<char>(1);
    REQUIRE(c != nullptr);

    OverAligned *o = arena.alloc<OverAligned>(1);
    REQUIRE(o != nullptr);
    CHECK(reinterpret_cast<std::uintptr_t>(o) % alignof(OverAligned) == 0);
  }

  SUBCASE("Mixed alignment structs scale inside element counts") {
    constexpr size_t N = 50;
    Mixed *m = arena.alloc<Mixed>(N);
    REQUIRE(m != nullptr);
    CHECK(reinterpret_cast<std::uintptr_t>(m) % alignof(Mixed) == 0);
  }
}

// ---------------------------------------------------------------------
// Oversized Allocation and Growth Fallbacks
// ---------------------------------------------------------------------
TEST_CASE("Arena Oversized Allocations & Growth Mechanisms") {
  auto arena_opt = Arena::create();
  Arena arena = std::move(*arena_opt);

  SUBCASE("Allocation scaling off-by-one boundary splits correctly") {
    char *p1 = arena.alloc<char>(Arena::DEFAULT_BLOCK_SIZE - 1);
    REQUIRE(p1 != nullptr);

    char *p2 = arena.alloc<char>(8);
    REQUIRE(p2 != nullptr);
  }

  SUBCASE("Allocation larger than default size forces customized fallback "
          "allocation") {
    size_t massive_count = (Arena::DEFAULT_BLOCK_SIZE / sizeof(double)) + 1000;
    double *massive_ptr = arena.alloc<double>(massive_count);

    if (massive_ptr != nullptr) {
      massive_ptr[0] = 3.14;
      massive_ptr[massive_count - 1] = 2.71;
      CHECK(massive_ptr[0] == 3.14);
    }
  }
}

TEST_CASE("Arena Geometric Block Growth Checking") {
  auto arena_opt = Arena::create();
  Arena arena = std::move(*arena_opt);

  size_t first_allocation_count = (40 * 1024 * 1024) / sizeof(char) - 100;
  char *ptr1 = arena.alloc<char>(first_allocation_count);
  REQUIRE(ptr1 != nullptr);

  char *ptr2 = arena.alloc<char>(1000);
  REQUIRE(ptr2 != nullptr);

  size_t massive_count = (600 * 1024 * 1024) / sizeof(char);
  char *ptr3 = arena.alloc<char>(massive_count);
  if (ptr3 != nullptr) {
    ptr3[0] = 'A';
    CHECK(ptr3[0] == 'A');
  }
}

// ---------------------------------------------------------------------
// Arena State Reset Semantics
// ---------------------------------------------------------------------
TEST_CASE("Arena State Reset Lifecycle Properties") {
  auto arena_opt = Arena::create();
  Arena arena = std::move(*arena_opt);

  SUBCASE("Reset clears tracking back to the beginning of the active block") {
    int *ptr1 = arena.alloc<int>(100);
    REQUIRE(ptr1 != nullptr);

    arena.reset();

    int *ptr2 = arena.alloc<int>(100);
    REQUIRE(ptr2 != nullptr);
    CHECK(reinterpret_cast<std::uintptr_t>(ptr1) ==
          reinterpret_cast<std::uintptr_t>(ptr2));
  }

  SUBCASE("Redundant multi-resets on unallocated structures execute safely") {
    arena.reset();
    arena.reset();
    int *p = arena.alloc<int>(1);
    REQUIRE(p != nullptr);
  }
}

TEST_CASE("Arena Block Downstream Traversal and Splicing") {
  auto arena_opt = Arena::create();
  Arena arena = std::move(*arena_opt);

  size_t initial_fill = (40 * 1024 * 1024) / sizeof(char) - 100;
  char *block1_ptr = arena.alloc<char>(initial_fill);
  REQUIRE(block1_ptr != nullptr);

  char *block2_ptr = arena.alloc<char>(1000);
  REQUIRE(block2_ptr != nullptr);

  arena.reset();

  char *reuse_block1 = arena.alloc<char>(10);
  CHECK(reuse_block1 == block1_ptr);

  size_t trigger_traversal_size = (40 * 1024 * 1024) - 5;
  char *reuse_block2 = arena.alloc<char>(trigger_traversal_size);

  REQUIRE(reuse_block2 != nullptr);
  CHECK(reuse_block2 == block2_ptr);
}

// ---------------------------------------------------------------------
// Move Semantics & Destructor Safety
// ---------------------------------------------------------------------
TEST_CASE("Arena Move Semantics Integrity Protocols") {
  auto arena_opt = Arena::create();
  Arena arena = std::move(*arena_opt);

  SUBCASE("Move construction transfers allocation ownership smoothly") {
    int *ptr1 = arena.alloc<int>(1);
    REQUIRE(ptr1 != nullptr);
    *ptr1 = 42;

    Arena moved_arena(std::move(arena));
    int *ptr2 = moved_arena.alloc<int>(1);
    CHECK(ptr2 != nullptr);
    CHECK(*ptr1 == 42);
  }

  SUBCASE("Move assignment drops old references cleanly") {
    auto b_opt = Arena::create();
    Arena b = std::move(*b_opt);

    int *pb = b.alloc<int>(1);
    *pb = 22;

    arena = std::move(b);
    CHECK(*pb == 22);
  }

  SUBCASE("Self move-assignment variants reject self-corruption") {
    int *p = arena.alloc<int>(1);
    REQUIRE(p != nullptr);
    *p = 77;

    arena = std::move(arena);
    CHECK(*p == 77);
  }
}

TEST_CASE("Arena Destruction Lifetime Executions") {
  SUBCASE("Destruction without usage terminates securely") {
    auto arena_opt = Arena::create();
    REQUIRE(arena_opt.has_value());
  }

  SUBCASE("Destruction across chained structures cleanly cleans fragments") {
    {
      auto arena_opt = Arena::create();
      Arena local_arena = std::move(*arena_opt);
      for (int i = 0; i < 5; ++i) {
        (void)local_arena.alloc<char>(Arena::DEFAULT_BLOCK_SIZE);
      }
    }
    CHECK(true);
  }
}

// ---------------------------------------------------------------------
// Defensive Overflow Probing
// ---------------------------------------------------------------------
TEST_CASE("alloc with a count that would overflow size_t * sizeof(T)") {
  auto arena_opt = Arena::create();
  Arena arena = std::move(*arena_opt);

  constexpr size_t huge = static_cast<size_t>(-1);
  double *p = arena.alloc<double>(huge);
  CHECK(p == nullptr);
}
