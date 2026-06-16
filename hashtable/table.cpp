#include "table.h"

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
