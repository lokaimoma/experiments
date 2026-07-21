#pragma once

#include <unistd.h>
#include <utility>
struct UniqueFd {
  int fd{-1};

  ~UniqueFd() { reset(); }

  UniqueFd() = default;
  explicit UniqueFd(int fd) : fd{fd} {}

  UniqueFd(const UniqueFd &) = delete;
  UniqueFd &operator=(const UniqueFd &) = delete;

  UniqueFd(UniqueFd &&u) noexcept : fd{u.release()} {}

  UniqueFd &operator=(UniqueFd &&u) noexcept {
    if (this != &u) {
      reset();
      fd = u.release();
    }
    return *this;
  }

  void reset(int f = -1) noexcept {
    if (fd != -1) {
      ::close(fd);
    }
    fd = f;
  }

  int release() noexcept { return std::exchange(fd, -1); };

  operator int() const { return fd; };
};
