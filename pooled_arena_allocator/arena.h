#pragma once
#include <cstddef>

namespace {
struct MemoryBlock {
  void *start{nullptr};
  void *end{nullptr};
  void *cur{nullptr};
  MemoryBlock *next{nullptr};
};
} // namespace

template <size_t DEFAULT_BLOCK_SIZE = 40 * 1024 * 1024> // 40 megabytes
class Arena {
private:
  MemoryBlock *head_of_blocks{nullptr};
  MemoryBlock *active_block{nullptr};
  Arena() = default;
  void clear();
  void request_new_block(size_t cap);

public:
  Arena(const Arena &) = delete;
  Arena &operator=(const Arena &) = delete;

  Arena(Arena &&) noexcept;
  Arena &operator=(Arena &&) noexcept;
  ~Arena();

  template <typename T> [[nodiscard]] T *alloc(size_t count);
  void reset();
};
