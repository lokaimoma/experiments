#include "table.h"
#include <utility>

Table::Table(size_t size, std::optional<Arena> &&arena_opt, Arena *active_arena)
    : size{size}, size_mask{size - 1}, owned_arena{std::move(arena_opt)},
      arenaptr{active_arena ? active_arena : &(*owned_arena)},
      tnodes([size, this] {
        TNode *nodes{arenaptr->alloc<TNode>(size)};
        if (!nodes) {
          throw std::bad_alloc();
        }
        return nodes;
      }()) {
  assert(size > 0 && ((size - 1) & (size)) == 0);
}

std::optional<Arena> Table::create_owned_arena(size_t size) {
  auto a{Arena::create(size)};
  if (!a.has_value()) {
    throw std::bad_alloc();
  }
  return std::move(a);
}

Table::Table(Table &&t) noexcept
    : size{t.size}, size_mask{t.size_mask},
      owned_arena{std::move(t.owned_arena)},
      arenaptr{owned_arena.has_value() ? &(*owned_arena) : t.arenaptr},
      tnodes{t.tnodes} {
  t.size = 0;
  t.size_mask = 0;
  t.arenaptr = nullptr;
  t.tnodes = nullptr;
}

Table &Table::operator=(Table &&t) noexcept {
  if (this != &t) {
    size = t.size;
    size_mask = t.size_mask;
    owned_arena = std::move(t.owned_arena);
    arenaptr = owned_arena.has_value() ? &(*owned_arena) : t.arenaptr;
    tnodes = t.tnodes;

    t.size = 0;
    t.size_mask = 0;
    t.arenaptr = nullptr;
    t.tnodes = nullptr;
  }

  return *this;
}
