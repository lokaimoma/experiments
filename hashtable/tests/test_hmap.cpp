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
