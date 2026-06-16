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
  size_t cap{0};
  size_t cap_mask{0}; // Must be a power of 2 (2^cap-1)
  std::optional<Arena> owned_arena;
  Arena *arenaptr{nullptr};
  TNode **tnodes{nullptr};

  Table(size_t cap, std::optional<Arena> &&arena_opt, Arena *active_arena);
  static std::optional<Arena> create_owned_arena(size_t size);

public:
  Table(size_t cap, Arena &arena) : Table(cap, std::nullopt, &arena) {}
  Table(size_t cap) : Table(cap, create_owned_arena(cap), nullptr) {}

  Table(Table &&t) noexcept;
  Table &operator=(Table &&t) noexcept;

  void insert(TNode *target);
  template <typename KeyEq> TNode *detach(TNode *target, KeyEq eqfn);
  template <typename KeyEq> TNode *lookup(TNode *target, KeyEq eqfn);

  TNode *&operator[](size_t idx);
  size_t get_size() const;
  size_t get_cap() const;
};

template <typename KeyEq> TNode *Table::detach(TNode *target, KeyEq eqfn) {
  size_t pos{target->hcode &
             cap_mask}; // hcode % cap (valid because cap is a power of 2)
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
  --size;
  return t;
}

template <typename KeyEq> TNode *Table::lookup(TNode *target, KeyEq eqfn) {
  size_t pos{target->hcode &
             cap_mask}; // hcode % cap (valid because cap is a power of 2)
  TNode *current{*(tnodes + pos)};
  while (current && !((current)->hcode == target->hcode && eqfn(current))) {
    current = current->next;
  }
  return current;
}
