#pragma once
#include <cstddef>
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
  static constexpr size_t DEFAULT_BLOCK_SIZE = 40 * 1024 * 1024;

  MemoryBlock *head_of_blocks{nullptr};
  MemoryBlock *active_block{nullptr};
  Arena() = default;
  void clear();
  void request_new_block(size_t cap);

public:
  Arena(const Arena &) = delete;
  Arena &operator=(const Arena &) = delete;

  static std::optional<Arena> create();
  Arena(Arena &&) noexcept;
  Arena &operator=(Arena &&) noexcept;
  ~Arena();

  template <typename T> [[nodiscard]] T *alloc(size_t count);
  void reset();
};
