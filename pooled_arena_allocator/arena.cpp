#include "arena.h"

void Arena::clear() {
  MemoryBlock *current{head_of_blocks};

  while (current) {
    MemoryBlock *next{current->next};
    if (current->start) {
      ::free(current->start);
    }
    delete current;
    current = next;
  }

  head_of_blocks = nullptr;
}

Arena::~Arena() { clear(); }

Arena::Arena(Arena &&a) noexcept
    : head_of_blocks{a.head_of_blocks}, active_block{a.active_block} {
  a.head_of_blocks = nullptr;
  a.active_block = nullptr;
}

Arena &Arena::operator=(Arena &&a) noexcept {
  if (this == &a) {
    return *this;
  }

  clear();

  this->head_of_blocks = a.head_of_blocks;
  this->active_block = a.active_block;

  a.head_of_blocks = nullptr;
  a.active_block = nullptr;

  return *this;
}
