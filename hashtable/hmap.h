#pragma once
#include "arena.h"
#include "hmaphasher.h"
#include "objectpool.h"
#include "table.h"
#include <cstring>
#include <new>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

#define container_of(ptr, T, member) \
  reinterpret_cast<T *>((char *)(ptr) - offsetof(T, member))

template <typename K, typename V> class HMap {
  using StorageK = std::conditional_t<std::is_same_v<K, std::string> ||
                                          std::is_same_v<K, std::string_view> ||
                                          std::is_same_v<K, const char *>,
                                      std::string_view, K>;

  struct Entry {
    StorageK key;
    V value;
    TNode hnode;
  };

private:
  static constexpr size_t k_rehash_work = 128;

  Arena arena;
  ObjectPool<HMap::Entry> objectPool;
  mutable Table htab_primary{1024};
  mutable std::optional<Table> htab_secondary;
  mutable size_t rehash_idx{0};
  mutable size_t live_count{0};

  StorageK internal_key(K key) {
    if constexpr (std::is_same_v<StorageK, std::string_view>) {
      std::string_view sv{key};
      if (sv.empty()) {
        return std::string_view{};
      }

      char *start_ptr{arena.alloc<char>(sv.size())};
      if (!start_ptr) {
        throw std::bad_alloc();
      }
      std::memcpy(start_ptr, sv.data(), sv.size());
      return std::string_view{start_ptr, sv.size()};
    } else {
      return key;
    }
  }

  bool is_rehashing() const;
  void perform_rehash() const;

public:
  HMap()
      : arena([]() {
          auto a = Arena::create();
          if (!a.has_value()) {
            throw std::bad_alloc();
          }
          return std::move(a.value());
        }()),
        objectPool(arena) {}
  void add(K key, V value);
  std::optional<V> remove(K key);
  std::optional<V> get(K key) const;
  size_t size() const { return live_count; }
};

template <typename K, typename V> void HMap<K, V>::add(K key, V value) {
  using MapEntry = typename HMap<K, V>::Entry;

  TNode lookupNode{.hcode{HMapHasher<K>::hash(key)}};

  TNode *n{htab_primary.lookup(&lookupNode, [key](TNode *l) {
    MapEntry *le{container_of(l, MapEntry, hnode)};
    return le->key == key;
  })};

  if (!n && is_rehashing()) {
    Table &t{htab_secondary.value()};
    n = t.lookup(&lookupNode, [key](TNode *l) {
      MapEntry *le{container_of(l, MapEntry, hnode)};
      return le->key == key;
    });
  }

  if (n) {
    MapEntry *ee{container_of(n, MapEntry, hnode)};
    ee->value = value;
    return;
  }

  MapEntry *e{objectPool.alloc()};
  ::new (e) MapEntry();
  e->key = internal_key(key);
  e->value = value;
  e->hnode = TNode{.hcode{HMapHasher<K>::hash(key)}, .next = nullptr};
  live_count++;

  if (!is_rehashing()) {
    htab_primary.insert(&e->hnode);
    size_t cap{htab_primary.get_cap()};

    if ((live_count * 4 > cap * 3)) { // 75% max load factor
      htab_secondary.emplace(cap * 2);
      rehash_idx = 0;
      perform_rehash();
    }
  } else {
    htab_secondary.value().insert(&e->hnode);
    perform_rehash();
  }
}

template <typename K, typename V> std::optional<V> HMap<K, V>::remove(K key) {
  using MapEntry = typename HMap<K, V>::Entry;
  TNode node{.hcode{HMapHasher<K>::hash(key)}};
  TNode *nn = htab_primary.detach(&node, [key](TNode *l) {
    MapEntry *le{container_of(l, MapEntry, hnode)};
    return le->key == key;
  });

  if (!nn && is_rehashing()) {
    Table &t{htab_secondary.value()};
    nn = t.detach(&node, [key](TNode *l) {
      MapEntry *le{container_of(l, MapEntry, hnode)};
      return le->key == key;
    });
  }

  perform_rehash();
  if (!nn) {
    return {};
  }

  MapEntry *e{container_of(nn, MapEntry, hnode)};
  V v{e->value};
  e->~MapEntry();
  objectPool.free(e);
  live_count--;

  return std::optional{v};
}

template <typename K, typename V>
std::optional<V> HMap<K, V>::get(K key) const {
  using MapEntry = typename HMap<K, V>::Entry;
  TNode node{.hcode{HMapHasher<K>::hash(key)}};

  perform_rehash();

  TNode *nn = htab_primary.lookup(&node, [key](TNode *l) {
    MapEntry *le{container_of(l, MapEntry, hnode)};
    return le->key == key;
  });

  if (!nn && is_rehashing()) {
    const Table &t{htab_secondary.value()};
    nn = t.lookup(&node, [key](TNode *l) {
      MapEntry *le{container_of(l, MapEntry, hnode)};
      return le->key == key;
    });
  }

  if (!nn) {
    return {};
  }

  return std::optional{container_of(nn, MapEntry, hnode)->value};
}

template <typename K, typename V> bool HMap<K, V>::is_rehashing() const {
  return htab_secondary.has_value();
}

template <typename K, typename V> void HMap<K, V>::perform_rehash() const {
  if (!is_rehashing()) {
    return;
  }

  Table &t{htab_secondary.value()};
  size_t primary_cap{htab_primary.get_cap()};
  for (size_t i{0}; i < k_rehash_work && i < primary_cap;) {
    if (rehash_idx >= primary_cap) {
      break;
    }
    TNode *&a{htab_primary[rehash_idx]};
    if (a) {
      ++i;
    }
    while (a) {
      TNode *next{a->next};
      a->next = nullptr;
      t.insert(a);
      a = next;
    }
    ++rehash_idx;
  }

  if (rehash_idx >= primary_cap) {
    htab_primary = std::move(htab_secondary.value());
    htab_secondary.reset();
    rehash_idx = 0;
  }
}
