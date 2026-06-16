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
  TNode **tnodes{nullptr};

  Table(size_t size, std::optional<Arena> &&arena_opt, Arena *active_arena);
  static std::optional<Arena> create_owned_arena(size_t size);

public:
  Table(size_t size, Arena &arena) : Table(size, std::nullopt, &arena) {}
  Table(size_t size) : Table(size, create_owned_arena(size), nullptr) {}

  Table(Table &&t) noexcept;
  Table &operator=(Table &&t) noexcept;

  void insert(TNode *target);
  template <typename KeyEq> TNode *detach(TNode *target, KeyEq eqfn);
  template <typename KeyEq> TNode *lookup(TNode *target, KeyEq eqfn);
};

template <typename KeyEq> TNode *Table::detach(TNode *target, KeyEq eqfn) {
  size_t pos{target->hcode &
             size_mask}; // hcode % size (valid because size is a power of 2)
  TNode **current{tnodes + pos};

  while (*current && !((*current)->hcode == target->hcode && eqfn(*current))) {
    current = &((*current)->next);
  }

  if (!*current) {
    return nullptr;
  }

  TNode *t{*current};
  *current = t->next;
  t->next = nullptr;
  return t;
}
