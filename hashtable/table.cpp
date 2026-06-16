#include "table.h"
#include <cassert>
#include <cstring>
#include <utility>

Table::Table(size_t cap, std::optional<Arena> &&arena_opt, Arena *active_arena)
    : cap{cap}, cap_mask{cap - 1}, owned_arena{std::move(arena_opt)},
      arenaptr{active_arena ? active_arena : &(*owned_arena)},
      tnodes([cap, this] {
        TNode **nodes{arenaptr->alloc<TNode *>(cap)};
        if (!nodes) {
          throw std::bad_alloc();
        }
        std::memset(nodes, 0, cap * sizeof(TNode *));
        return nodes;
      }()) {
  assert(cap > 0 && ((cap - 1) & (cap)) == 0);
}

std::optional<Arena> Table::create_owned_arena(size_t cap) {
  auto a{Arena::create(cap)};
  if (!a.has_value()) {
    throw std::bad_alloc();
  }
  return std::move(a);
}

Table::Table(Table &&t) noexcept
    : size{t.size}, cap{t.cap}, cap_mask{t.cap_mask},
      owned_arena{std::move(t.owned_arena)},
      arenaptr{owned_arena.has_value() ? &(*owned_arena) : t.arenaptr},
      tnodes{t.tnodes} {
  t.size = 0;
  t.cap = 0;
  t.cap_mask = 0;
  t.arenaptr = nullptr;
  t.tnodes = nullptr;
}

Table &Table::operator=(Table &&t) noexcept {
  if (this != &t) {
    size = t.size;
    cap = t.cap;
    cap_mask = t.cap_mask;
    owned_arena = std::move(t.owned_arena);
    arenaptr = owned_arena.has_value() ? &(*owned_arena) : t.arenaptr;
    tnodes = t.tnodes;

    t.size = 0;
    t.cap = 0;
    t.cap_mask = 0;
    t.arenaptr = nullptr;
    t.tnodes = nullptr;
  }

  return *this;
}

void Table::insert(TNode *target) {
  size_t pos{target->hcode & cap_mask}; // hcode % cap
  target->next = *(tnodes + pos);
  *(tnodes + pos) = target;
  ++size;
}

TNode *&Table::operator[](size_t idx) {
  if (idx >= cap) {
    throw std::out_of_range("Table index out of bounds");
  }
  return tnodes[idx];
}

size_t Table::len() const { return size; }
