#include <stdexcept>
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "../table.h"
#include "doctest.h"
#include <string>

// Custom node wrapper to bind data to your bare TNode structure
struct TestNode : public TNode {
  std::string key;
  std::string value;
};

// Clean equality helper factory
auto make_eq(const std::string &target_key) {
  return [target_key](TNode *node) {
    return static_cast<TestNode *>(node)->key == target_key;
  };
}

// Simple deterministic hash helper
uint32_t mock_hash(const std::string &str) {
  uint32_t hash = 2166136261U;
  for (char c : str) {
    hash ^= static_cast<uint32_t>(c);
    hash *= 16777619U;
  }
  return hash;
}

TEST_SUITE("Table - Core Functionality") {

  TEST_CASE("Basic Insertion and Lookup Operations") {
    Table table(16);
    CHECK(table.get_size() == 0); // Initially empty

    TestNode n1;
    n1.hcode = mock_hash("apple");
    n1.key = "apple";
    n1.value = "fruit";

    table.insert(&n1);
    CHECK(table.get_size() == 1); // Size must increment on insert

    TNode *found = table.lookup(&n1, make_eq("apple"));
    REQUIRE(found != nullptr);
    CHECK(static_cast<TestNode *>(found)->value == "fruit");
  }

  TEST_CASE("Lookup Missing Keys Safely") {
    Table table(16);

    TestNode n1;
    n1.hcode = mock_hash("apple");
    n1.key = "apple";

    CHECK(table.lookup(&n1, make_eq("apple")) == nullptr);

    table.insert(&n1);
    CHECK(table.get_size() == 1);

    TestNode dummy;
    dummy.hcode = mock_hash("banana");
    dummy.key = "banana";
    CHECK(table.lookup(&dummy, make_eq("banana")) == nullptr);
    CHECK(table.get_size() == 1); // Failed lookup must not modify size
  }

  TEST_CASE("Same Hash Code but Different Keys (Short-Circuit Evaluation)") {
    Table table(16);

    TestNode n1, n2;
    n1.hcode = 42;
    n1.key = "RealKey";
    n2.hcode = 42;
    n2.key = "ImposterKey";

    table.insert(&n1);
    CHECK(table.get_size() == 1);

    CHECK(table.lookup(&n2, make_eq("ImposterKey")) == nullptr);
  }
}

TEST_SUITE("Table - Collision & Pointer Mechanics") {

  TEST_CASE("Hash Collisions Chain Accurately") {
    Table table(4);

    TestNode n1, n2, n3;
    n1.hcode = 1;
    n1.key = "node1";
    n2.hcode = 5;
    n2.key = "node2";
    n3.hcode = 9;
    n3.key = "node3";

    table.insert(&n1);
    table.insert(&n2);
    table.insert(&n3);
    CHECK(table.get_size() ==
          3); // Collisions must track accurate overall counts

    CHECK(table.lookup(&n1, make_eq("node1")) == &n1);
    CHECK(table.lookup(&n2, make_eq("node2")) == &n2);
    CHECK(table.lookup(&n3, make_eq("node3")) == &n3);

    CHECK(n3.next == &n2);
    CHECK(n2.next == &n1);
    CHECK(n1.next == nullptr);
  }

  TEST_CASE("Detaching Nodes Handles Layered Edge Cases Perfectly") {
    Table table(4);

    TestNode n1, n2, n3;
    n1.hcode = 5;
    n1.key = "A";
    n2.hcode = 5;
    n2.key = "B";
    n3.hcode = 5;
    n3.key = "C";

    table.insert(&n1);
    table.insert(&n2);
    table.insert(&n3);
    CHECK(table.get_size() == 3);

    SUBCASE("Detaching a non-existent key") {
      TestNode fake;
      fake.hcode = 5;
      fake.key = "FAKE";
      CHECK(table.detach(&fake, make_eq("FAKE")) == nullptr);
      CHECK(table.get_size() ==
            3); // Unsuccessful detach must leave size unaffected
    }

    SUBCASE("Detaching from the middle of a bucket chain") {
      TNode *detached = table.detach(&n2, make_eq("B"));
      REQUIRE(detached == &n2);
      CHECK(table.get_size() == 2); // Decrement tracking check
      CHECK(n2.next == nullptr);
      CHECK(n3.next == &n1);
    }

    SUBCASE("Detaching straight from the bucket head pointer") {
      TNode *detached = table.detach(&n3, make_eq("C"));
      REQUIRE(detached == &n3);
      CHECK(table.get_size() == 2); // Decrement tracking check
      CHECK(n3.next == nullptr);
      CHECK(table.lookup(&n3, make_eq("C")) == nullptr);
      CHECK(table.lookup(&n2, make_eq("B")) == &n2);
    }

    SUBCASE("Detaching the absolute final tail element in a chain") {
      table.detach(&n3, make_eq("C"));
      table.detach(&n2, make_eq("B"));
      CHECK(table.get_size() == 1);

      TNode *detached = table.detach(&n1, make_eq("A"));
      REQUIRE(detached == &n1);
      CHECK(table.get_size() == 0); // Empty table boundary state verified
      CHECK(n1.next == nullptr);
      CHECK(table.lookup(&n1, make_eq("A")) == nullptr);
    }
  }
}

TEST_SUITE("Table - Direct Bucket Indexing") {

  TEST_CASE("Bucket Access and Direct Mutation via operator[]") {
    Table table(4);

    TestNode n1, n2;
    n1.hcode = 1;
    n1.key = "node1";
    n2.hcode = 1;
    n2.key = "node2";

    table.insert(&n1);
    table.insert(&n2);
    CHECK(table.get_size() == 2);

    // Test Case 1: Read bucket state via operator[]
    TNode *bucket_head = table[1];
    REQUIRE(bucket_head == &n2);
    CHECK(bucket_head->next == &n1);

    // Test Case 2: Verify empty bucket returns clean nullptr from memset
    // tracking
    CHECK(table[0] == nullptr);
    CHECK(table[2] == nullptr);
    CHECK(table[3] == nullptr);

    // Test Case 3: Mutate bucket pointer via Reference (Simulating HMap
    // progressive rehash pluck) NOTE: Direct pointer erasure bypassing detach()
    // doesn't automatically alter size internally!
    table[1] = nullptr;

    CHECK(table.lookup(&n1, make_eq("node1")) == nullptr);
    CHECK(table.lookup(&n2, make_eq("node2")) == nullptr);
    CHECK(n2.next == &n1);
  }

  TEST_CASE("Out of bounds bucket indexing triggers exception") {
    Table table(4);
    // Verified against your new std::out_of_range exception switch
    CHECK_THROWS_AS(table[4], std::out_of_range);
  }
}

TEST_SUITE("Table - Lifecycle & Memory State Verification") {

  TEST_CASE("Move Constructor transfers ownership securely") {
    Table source_table(16);
    TestNode n1;
    n1.hcode = mock_hash("move_me");
    n1.key = "move_me";
    source_table.insert(&n1);
    REQUIRE(source_table.get_size() == 1);

    Table moved_table(std::move(source_table));
    CHECK(moved_table.get_size() ==
          1); // Size value metadata has moved over cleanly

    CHECK(moved_table.lookup(&n1, make_eq("move_me")) == &n1);
  }

  TEST_CASE("Move Assignment Operator cleans old memory and updates state") {
    Table source_table(16);
    TestNode n1;
    n1.hcode = mock_hash("assign_me");
    n1.key = "assign_me";
    source_table.insert(&n1);
    REQUIRE(source_table.get_size() == 1);

    Table assigned_table(16);
    CHECK(assigned_table.get_size() == 0);

    assigned_table = std::move(source_table);
    CHECK(assigned_table.get_size() ==
          1); // Target destination holds incoming size accurately

    CHECK(assigned_table.lookup(&n1, make_eq("assign_me")) == &n1);
  }

  TEST_CASE("Capacity properties are tracked and preserved accurately") {
    Table table(32);
    CHECK(table.get_cap() == 32);

    // Verify move mechanics transfer capacity metadata without corruption
    Table moved_table(std::move(table));
    CHECK(moved_table.get_cap() == 32);
  }
}
