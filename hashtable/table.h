#pragma once
#include "arena.h"
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <optional>

struct TNode {
  uint32_t hcode;
  struct TNode *next{nullptr};
};

class Table {
private:
  size_t size{0};
  size_t size_mask{0}; // Must be a power of 2 (2^size-1)
  std::optional<Arena> owned_arena;
  Arena *arenaptr{nullptr};
  TNode *tnodes{nullptr};

  Table(size_t size, std::optional<Arena> &&arena_opt, Arena *active_arena);
  static std::optional<Arena> create_owned_arena(size_t size);

public:
  Table(size_t size, Arena &arena) : Table(size, std::nullopt, &arena) {}

  Table(size_t size) : Table(size, create_owned_arena(size), nullptr) {}
};
