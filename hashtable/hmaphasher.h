#pragma once

// After this line everything below is AI-Generated

#include <bit> // Required for std::bit_cast (C++20)
#include <cstddef>
#include <cstdint>
#include <string>
#include <type_traits>

// A trait to see if a type is a string variant
template <typename T> struct is_string_type : std::false_type {};

template <> struct is_string_type<std::string> : std::true_type {};
template <> struct is_string_type<std::string_view> : std::true_type {};
template <> struct is_string_type<const char *> : std::true_type {};
template <size_t N> struct is_string_type<char[N]> : std::true_type {};

// The primary gatekeeper routes all string-like types to std::string_view
template <typename KT> struct HMapHasher {
  using TargetType = typename std::conditional<is_string_type<KT>::value,
                                               std::string_view, KT>::type;

  static uint32_t hash(const KT &key) {
    if constexpr (std::is_same_v<KT, TargetType>) {
      // If KT matches TargetType, it means we are working with a type that
      // has an explicit template specialization defined below (like int, float,
      // or std::string_view). However, because those definitions are lower down
      // in the file, we must forward to them dynamically to avoid compiler
      // resolution issues.
      return HMapHasher<TargetType>::hash(key);
    } else {
      // If KT is a string variant (std::string, const char*, char[N]),
      // TargetType is std::string_view. We explicitly cast the key to
      // std::string_view here to match the signature below.
      return HMapHasher<TargetType>::hash(std::string_view(key));
    }
  }
};

// ============================================================================
// Shared Core Processing Utilities (Private / Helper Logic)
// ============================================================================
struct FNVConstants {
  static constexpr uint32_t PRIME = 0x01000193;
  static constexpr uint32_t OFFSET_BASIS = 0x811C9DC5;
};

// Hashes a raw 32-bit block entirely inside registers via bit-shifting
inline uint32_t hash_32_bits(uint32_t bits) {
  uint32_t h = FNVConstants::OFFSET_BASIS;
  h = (h ^ (bits & 0xFF)) * FNVConstants::PRIME;
  h = (h ^ ((bits >> 8) & 0xFF)) * FNVConstants::PRIME;
  h = (h ^ ((bits >> 16) & 0xFF)) * FNVConstants::PRIME;
  h = (h ^ ((bits >> 24) & 0xFF)) * FNVConstants::PRIME;
  return h;
}

// ============================================================================
// Explicit Specializations
// ============================================================================

// --- INTEGER HASHER ---
template <> struct HMapHasher<int> {
  static uint32_t hash(int key) {
    // Cast directly to unsigned representation to safely preserve bit shifts
    return hash_32_bits(static_cast<uint32_t>(key));
  }
};

// --- STRING VIEW HASHER ---
// This safely handles std::string, std::string_view, and const char*
template <> struct HMapHasher<std::string_view> {
  static uint32_t hash(std::string_view key) {
    uint32_t h = FNVConstants::OFFSET_BASIS;
    const uint8_t *bytes = reinterpret_cast<const uint8_t *>(key.data());
    size_t len = key.size();

    for (size_t i = 0; i < len; ++i) {
      h = (h ^ bytes[i]) * FNVConstants::PRIME;
    }
    return h;
  }
};

// --- FLOAT HASHER ---
template <> struct HMapHasher<float> {
  static uint32_t hash(float key) {
    // Normalization Step 1: Clean up -0.0f mismatch
    if (key == 0.0f) {
      key = 0.0f;
    }

    // Normalization Step 2: Extract raw bits without memory aliasing bugs
    uint32_t bits = std::bit_cast<uint32_t>(key);
    return hash_32_bits(bits);
  }
};

// --- DOUBLE HASHER ---
template <> struct HMapHasher<double> {
  static uint32_t hash(double key) {
    // Normalization Step 1: Clean up -0.0 mismatch
    if (key == 0.0) {
      key = 0.0;
    }

    // Normalization Step 2: Extract 64-bit layout
    uint64_t bits = std::bit_cast<uint64_t>(key);

    // Shift and process all 8 bytes sequentially
    uint32_t h = FNVConstants::OFFSET_BASIS;
    h = (h ^ (bits & 0xFF)) * FNVConstants::PRIME;
    h = (h ^ ((bits >> 8) & 0xFF)) * FNVConstants::PRIME;
    h = (h ^ ((bits >> 16) & 0xFF)) * FNVConstants::PRIME;
    h = (h ^ ((bits >> 24) & 0xFF)) * FNVConstants::PRIME;
    h = (h ^ ((bits >> 32) & 0xFF)) * FNVConstants::PRIME;
    h = (h ^ ((bits >> 40) & 0xFF)) * FNVConstants::PRIME;
    h = (h ^ ((bits >> 48) & 0xFF)) * FNVConstants::PRIME;
    h = (h ^ ((bits >> 56) & 0xFF)) * FNVConstants::PRIME;
    return h;
  }
};

