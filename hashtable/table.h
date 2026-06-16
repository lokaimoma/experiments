#pragma once
#include "arena.h"
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <new>
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

  Table(size_t size, std::optional<Arena> &&arena_opt, Arena *active_arena)
      : size{size}, size_mask{size - 1}, owned_arena{std::move(arena_opt)},
        arenaptr{active_arena}, tnodes([size, this] {
          TNode *nodes{arenaptr->alloc<TNode>(size)};
          if (!nodes) {
            throw std::bad_alloc();
          }
          return nodes;
        }()) {
    assert(size > 0 && ((size - 1) & (size)) == 0);
  }

  static std::optional<Arena> create_owned_arena() {
    auto a{Arena::create()};
    if (!a.has_value()) {
      throw std::bad_alloc();
    }
    return std::move(a);
  }

public:
  Table(size_t size, Arena &arena) : Table(size, std::nullopt, &arena) {}

  Table(size_t size) : Table(size, create_owned_arena(), nullptr) {
    arenaptr = &(*owned_arena);

    tnodes = arenaptr->alloc<TNode>(size);
    if (!tnodes) {
      throw std::bad_alloc();
    }
  }
};
