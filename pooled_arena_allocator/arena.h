#pragma once
#include <cstddef>
#include <limits>
#include <memory>
#include <optional>

namespace {
struct MemoryBlock {
  void *start{nullptr};
  void *end{nullptr};
  void *cur{nullptr};
  MemoryBlock *next{nullptr};
};
} // namespace

class Arena {
private:
  MemoryBlock *head_of_blocks{nullptr};
  MemoryBlock *active_block{nullptr};
  Arena() = default;
  void clear();
  bool request_new_block(size_t cap);

public:
  static constexpr size_t DEFAULT_BLOCK_SIZE = 40 * 1024 * 1024;

  Arena(const Arena &) = delete;
  Arena &operator=(const Arena &) = delete;

  static std::optional<Arena> create();
  Arena(Arena &&) noexcept;
  Arena &operator=(Arena &&) noexcept;
  ~Arena();

  template <typename T> [[nodiscard]] T *alloc(size_t count = 1);
  void reset();
};

template <typename T> [[nodiscard]] T *Arena::alloc(size_t count) {
  if (!active_block || count <= 0) {
    return nullptr;
  }

  if (count > std::numeric_limits<size_t>::max() / sizeof(T)) {
    return nullptr;
  }

  size_t req_cap{sizeof(T) * count};
  size_t avail_mem{static_cast<size_t>(static_cast<char *>(active_block->end) -
                                       static_cast<char *>(active_block->cur))};

  void *curpt_ptr{active_block->cur};
  void *ptr{std::align(alignof(T), req_cap, curpt_ptr, avail_mem)};

  if (!ptr) {
    if (!request_new_block(req_cap > Arena::DEFAULT_BLOCK_SIZE
                               ? req_cap
                               : Arena::DEFAULT_BLOCK_SIZE)) {
      return nullptr;
    }
    return alloc<T>(count);
  }

  active_block->cur = static_cast<char *>(curpt_ptr) + req_cap;
  return static_cast<T *>(ptr);
}
