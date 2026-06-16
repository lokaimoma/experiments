// AI-GENERATED
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

    TestNode n1;
    n1.hcode = mock_hash("apple");
    n1.key = "apple";
    n1.value = "fruit";

    table.insert(&n1);

    TNode *found = table.lookup(&n1, make_eq("apple"));
    REQUIRE(found != nullptr);
    CHECK(static_cast<TestNode *>(found)->value == "fruit");
  }

  TEST_CASE("Lookup Missing Keys Safely") {
    Table table(16);

    TestNode n1;
    n1.hcode = mock_hash("apple");
    n1.key = "apple";

    // Verifies the memset zero-initialization works (doesn't read garbage
    // pointer addresses)
    CHECK(table.lookup(&n1, make_eq("apple")) == nullptr);

    table.insert(&n1);

    TestNode dummy;
    dummy.hcode = mock_hash("banana");
    dummy.key = "banana";
    CHECK(table.lookup(&dummy, make_eq("banana")) == nullptr);
  }

  TEST_CASE("Same Hash Code but Different Keys (Short-Circuit Evaluation)") {
    Table table(16);

    TestNode n1, n2;
    n1.hcode = 42;
    n1.key = "RealKey";
    n2.hcode = 42;
    n2.key = "ImposterKey"; // Matching hash, different key content

    table.insert(&n1);

    // Verification that matching hash doesn't trick the lookup function
    CHECK(table.lookup(&n2, make_eq("ImposterKey")) == nullptr);
  }
}

TEST_SUITE("Table - Collision & Pointer Mechanics") {

  TEST_CASE("Hash Collisions Chain Accurately") {
    Table table(4); // Small capacity guarantees collisions

    TestNode n1, n2, n3;
    n1.hcode = 1;
    n1.key = "node1";
    n2.hcode = 5;
    n2.key = "node2"; // 5 & 3 == 1 (Bucket 1 Collision)
    n3.hcode = 9;
    n3.key = "node3"; // 9 & 3 == 1 (Bucket 1 Collision)

    table.insert(&n1);
    table.insert(&n2);
    table.insert(&n3);

    // All nodes must be discoverable independently via linear traversal
    CHECK(table.lookup(&n1, make_eq("node1")) == &n1);
    CHECK(table.lookup(&n2, make_eq("node2")) == &n2);
    CHECK(table.lookup(&n3, make_eq("node3")) == &n3);

    // Verify physical backward-chaining pointer structure (Head Insertion
    // layout)
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
    table.insert(
        &n3); // Table state at Bucket Index 1: Head -> C -> B -> A -> nullptr

    SUBCASE("Detaching a non-existent key") {
      TestNode fake;
      fake.hcode = 5;
      fake.key = "FAKE";
      CHECK(table.detach(&fake, make_eq("FAKE")) == nullptr);
    }

    SUBCASE("Detaching from the middle of a bucket chain") {
      TNode *detached = table.detach(&n2, make_eq("B"));
      REQUIRE(detached == &n2);
      CHECK(n2.next == nullptr); // Check detached node is safely isolated
      CHECK(n3.next ==
            &n1); // Check preceding node stitched safely past B directly to A
    }

    SUBCASE("Detaching straight from the bucket head pointer") {
      TNode *detached = table.detach(&n3, make_eq("C"));
      REQUIRE(detached == &n3);
      CHECK(n3.next == nullptr);
      CHECK(table.lookup(&n3, make_eq("C")) == nullptr);
      CHECK(table.lookup(&n2, make_eq("B")) ==
            &n2); // Ensure chain remnants remain intact
    }

    SUBCASE("Detaching the absolute final tail element in a chain") {
      table.detach(&n3, make_eq("C"));
      table.detach(&n2, make_eq("B"));

      TNode *detached = table.detach(&n1, make_eq("A"));
      REQUIRE(detached == &n1);
      CHECK(n1.next == nullptr);
      CHECK(table.lookup(&n1, make_eq("A")) == nullptr);
    }
  }
}

TEST_SUITE("Table - Lifecycle & Memory State Verification") {

  TEST_CASE("Move Constructor transfers ownership securely") {
    Table source_table(16);
    TestNode n1;
    n1.hcode = mock_hash("move_me");
    n1.key = "move_me";
    source_table.insert(&n1);

    Table moved_table(std::move(source_table));

    // Assert old reference lookup works perfectly within the new container
    // context
    CHECK(moved_table.lookup(&n1, make_eq("move_me")) == &n1);
  }

  TEST_CASE("Move Assignment Operator cleans old memory and updates state") {
    Table source_table(16);
    TestNode n1;
    n1.hcode = mock_hash("assign_me");
    n1.key = "assign_me";
    source_table.insert(&n1);

    Table assigned_table(16);
    assigned_table = std::move(source_table);

    CHECK(assigned_table.lookup(&n1, make_eq("assign_me")) == &n1);
  }
}
