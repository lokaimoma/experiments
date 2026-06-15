#pragma once

#include "arena.h"
template <typename T> class ObjectPool {
private:
  Arena &arena;
  void *head_of_free_list{nullptr};

public:
  explicit ObjectPool<T>(Arena &arena) : arena(arena) {}

  void free(T *ptr) {
    if (!ptr) {
      return;
    }

    void **pptr{reinterpret_cast<void **>(ptr)};
    *pptr = head_of_free_list;

    head_of_free_list = ptr;
  }

  void reset() {
    arena.reset();
    head_of_free_list = nullptr;
  }

  T *alloc() {
    if (head_of_free_list) {
      void *recycled{head_of_free_list};
      head_of_free_list = *reinterpret_cast<void **>(recycled);
      return reinterpret_cast<T *>(recycled);
    }

    return arena.alloc<T>();
  }
};
