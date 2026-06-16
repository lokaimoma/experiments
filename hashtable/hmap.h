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

#define container_of(ptr, T, member)                                           \
  ({                                                                           \
    const typeof(((T *)0)->member) *__mptr = (ptr);                            \
    (T *)((char *)__mptr - offsetof(T, member));                               \
  })

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
  Arena arena;
  ObjectPool<HMap::Entry> objectPool;
  Table htab{1024};

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
  std::optional<V> get(K key);
};

template <typename K, typename V> void HMap<K, V>::add(K key, V value) {
  using MapEntry = typename HMap<K, V>::Entry;

  TNode lookupNode{.hcode{HMapHasher<K>::hash(key)}};

  ;
  TNode *n{htab.lookup(&lookupNode, [key](TNode *l) {
    MapEntry *le{container_of(l, MapEntry, hnode)};
    return le->key == key;
  })};

  if (n) {
    MapEntry *ee{container_of(n, MapEntry, hnode)};
    ee->value = value;
  } else {
    MapEntry *e{objectPool.alloc()};
    ::new (e) MapEntry();
    e->key = internal_key(key);
    e->value = value;
    e->hnode = TNode{.hcode{HMapHasher<K>::hash(key)}, .next = nullptr};
    htab.insert(&e->hnode);
  }
}

template <typename K, typename V> std::optional<V> HMap<K, V>::remove(K key) {
  using MapEntry = typename HMap<K, V>::Entry;
  TNode node{.hcode{HMapHasher<K>::hash(key)}};
  TNode *nn = htab.detach(&node, [key](TNode *l) {
    MapEntry *le{container_of(l, MapEntry, hnode)};
    return le->key == key;
  });

  if (!nn) {
    return {};
  }

  MapEntry *e{container_of(nn, MapEntry, hnode)};
  V v{e->value};
  e->~MapEntry();
  objectPool.free(e);

  return std::optional{v};
}

template <typename K, typename V> std::optional<V> HMap<K, V>::get(K key) {
  using MapEntry = typename HMap<K, V>::Entry;
  TNode node{.hcode{HMapHasher<K>::hash(key)}};
  TNode *nn = htab.lookup(&node, [key](TNode *l) {
    MapEntry *le{container_of(l, MapEntry, hnode)};
    return le->key == key;
  });

  if (!nn) {
    return {};
  }

  return std::optional{container_of(nn, MapEntry, hnode)->value};
}
