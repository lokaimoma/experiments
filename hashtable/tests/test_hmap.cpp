#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "../hmap.h"
#include "doctest.h"
#include <string>

TEST_CASE("HMap Primitive Operations") {
  HMap<int, std::string> map;

  SUBCASE("Lookup on empty map returns std::nullopt") {
    CHECK_FALSE(map.get(42).has_value());
  }

  SUBCASE("Add insertions and lookups") {
    map.add(1, "one");
    map.add(2, "two");

    auto val1 = map.get(1);
    REQUIRE(val1.has_value());
    CHECK(*val1 == "one");

    auto val2 = map.get(2);
    REQUIRE(val2.has_value());
    CHECK(*val2 == "two");
  }

  SUBCASE("Overwriting an existing key updates its value inline") {
    map.add(42, "original");
    map.add(42, "updated");

    auto val = map.get(42);
    REQUIRE(val.has_value());
    CHECK(*val == "updated");
  }

  SUBCASE("Removing a key yields its value and detaches it from lookup") {
    map.add(99, "retire");

    auto removed_val = map.remove(99);
    REQUIRE(removed_val.has_value());
    CHECK(*removed_val == "retire");

    CHECK_FALSE(map.get(99).has_value());
  }

  SUBCASE("Removing a non-existent key returns std::nullopt") {
    CHECK_FALSE(map.remove(100).has_value());
  }
}

TEST_CASE("HMap String Key Interoperability and Arena Storage Allocation") {
  SUBCASE("Handling std::string types with stable internal arena copying") {
    HMap<std::string, int> map;

    {
      std::string transient_key = "temporary_scope_key_string";
      map.add(transient_key, 1234);
    }

    auto val = map.get("temporary_scope_key_string");
    REQUIRE(val.has_value());
    CHECK(*val == 1234);
  }

  SUBCASE("Handling const char* string literal keys") {
    HMap<const char *, int> map;

    map.add("alpha", 10);
    map.add("beta", 20);

    auto v1 = map.get("alpha");
    REQUIRE(v1.has_value());
    CHECK(*v1 == 10);

    auto removed = map.remove("alpha");
    REQUIRE(removed.has_value());
    CHECK(*removed == 10);

    CHECK_FALSE(map.get("alpha").has_value());
  }
}

TEST_CASE("HMap Collision Stability and ObjectPool Recycling") {
  HMap<int, int> map;

  SUBCASE("ObjectPool correctly recycles entry allocations after removals") {
    map.add(10, 100);
    map.remove(10);

    map.add(10, 200);
    auto val = map.get(10);
    REQUIRE(val.has_value());
    CHECK(*val == 200);
  }
}

// ── Rehashing tests ──────────────────────────────────────────────────────
//
// Initial capacity is 1024.  75% load factor → rehashing starts at entry #769.
// Each add/remove during rehasing moves 128 buckets (k_rehash_work).
// 1024 / 128 = 8 operations needed to complete the rehash.

TEST_CASE("HMap Rehashing Lifecycle") {
  constexpr int kRehashThreshold = 769;

  SUBCASE("Trigger rehash and verify all entries are findable") {
    HMap<int, int> map;
    for (int i = 0; i < kRehashThreshold; i++) {
      map.add(i, i);
    }
    for (int i = 0; i < kRehashThreshold; i++) {
      auto val = map.get(i);
      REQUIRE(val.has_value());
      CHECK(*val == i);
    }
  }

  SUBCASE("Get entries from primary and secondary during active rehashing") {
    HMap<int, int> map;
    for (int i = 0; i < kRehashThreshold + 10; i++) {
      map.add(i, i * 2);
    }
    CHECK(*map.get(0) == 0);
    CHECK(*map.get(kRehashThreshold / 2) == (kRehashThreshold / 2) * 2);
    CHECK(*map.get(kRehashThreshold - 1) == (kRehashThreshold - 1) * 2);
    CHECK(*map.get(kRehashThreshold + 9) == (kRehashThreshold + 9) * 2);
  }

  SUBCASE("Remove entries during rehashing") {
    HMap<int, int> map;
    for (int i = 0; i < kRehashThreshold; i++) {
      map.add(i, i * 3);
    }
    auto early = map.remove(0);
    REQUIRE(early.has_value());
    CHECK(*early == 0);
    CHECK_FALSE(map.get(0).has_value());

    auto late = map.remove(kRehashThreshold - 1);
    REQUIRE(late.has_value());
    CHECK(*late == (kRehashThreshold - 1) * 3);
    CHECK_FALSE(map.get(kRehashThreshold - 1).has_value());
  }

  SUBCASE("Update existing keys during rehashing") {
    HMap<int, int> map;
    for (int i = 0; i < kRehashThreshold; i++) {
      map.add(i, i);
    }
    map.add(0, 999);
    map.add(500, 888);
    map.add(kRehashThreshold - 1, 777);

    CHECK(*map.get(0) == 999);
    CHECK(*map.get(500) == 888);
    CHECK(*map.get(kRehashThreshold - 1) == 777);
  }

  SUBCASE("Adding new keys during rehashing") {
    HMap<int, int> map;
    for (int i = 0; i < kRehashThreshold; i++) {
      map.add(i, i);
    }
    map.add(kRehashThreshold + 100, 42);
    map.add(kRehashThreshold + 200, 84);

    CHECK(*map.get(kRehashThreshold + 100) == 42);
    CHECK(*map.get(kRehashThreshold + 200) == 84);
  }

  SUBCASE("Non-existent key operations during rehashing") {
    HMap<int, int> map;
    for (int i = 0; i < kRehashThreshold; i++) {
      map.add(i, i);
    }
    CHECK_FALSE(map.get(-1).has_value());
    CHECK_FALSE(map.remove(-1).has_value());
  }

  SUBCASE("ObjectPool recycling during rehashing") {
    HMap<int, int> map;
    for (int i = 0; i < kRehashThreshold; i++) {
      map.add(i, i);
    }
    map.remove(0);
    map.add(0, 999);
    CHECK(*map.get(0) == 999);
  }

  SUBCASE("Rehashing completes after sufficient operations") {
    HMap<int, int> map;
    for (int i = 0; i < kRehashThreshold; i++) {
      map.add(i, i);
    }
    // 8 calls to perform_rehash() needed to move 1024 buckets in batches of 128.
    // Each remove() during rehashing triggers one batch.  We insert
    // 20 non-existent removes to drive rehashing to completion.
    for (int i = 0; i < 20; i++) {
      map.remove(kRehashThreshold + i);
    }
    // Rehashing is now complete.  Original entries must still be intact.
    for (int i = 0; i < kRehashThreshold; i++) {
      auto val = map.get(i);
      REQUIRE(val.has_value());
      CHECK(*val == i);
    }
  }
}
