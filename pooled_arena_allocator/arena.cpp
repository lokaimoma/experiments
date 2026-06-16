#include "arena.h"
#include <cstdio>
#include <cstdlib>
#include <optional>

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

void Arena::reset() {
  if (!head_of_blocks || !head_of_blocks->start) {
    return;
  }

  MemoryBlock *current{head_of_blocks};
  while (current) {
    current->cur = current->start;
    current = current->next;
  }

  active_block = head_of_blocks;
}

std::optional<Arena> Arena::create(size_t s) {
  Arena a{};
  if (!a.request_new_block(s)) {
    return std::nullopt;
  }
  return a;
}

bool Arena::request_new_block(size_t cap) {
  if (active_block && active_block->next) {
    MemoryBlock *prev{active_block};
    MemoryBlock *cur{active_block->next};

    while (cur) {
      size_t _cap{static_cast<size_t>(static_cast<char *>(cur->end) -
                                      static_cast<char *>(cur->start))};

      if (_cap >= cap) {
        if (prev != active_block) {
          prev->next = cur->next;
          cur->next = active_block->next;
          active_block->next = cur;
        }
        active_block = cur;
        return true;
      }
      prev = cur;
      cur = cur->next;
    }
  }

  MemoryBlock *block{new MemoryBlock{}};
  block->start = malloc(cap);

  if (!block->start) {
    perror("request_new_block");
    delete block;
    return false;
  }

  block->cur = block->start;
  block->end = static_cast<char *>(block->start) + cap;

  if (!head_of_blocks) {
    head_of_blocks = block;
    active_block = block;
    return true;
  }

  block->next = active_block->next;
  active_block->next = block;
  active_block = block;
  return true;
}
