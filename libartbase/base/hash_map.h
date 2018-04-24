/*
 * Copyright (C) 2014 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef ART_LIBARTBASE_BASE_HASH_MAP_H_
#define ART_LIBARTBASE_BASE_HASH_MAP_H_

#include <utility>

#include "hash_set.h"

namespace art {

template <typename Fn>
class HashMapWrapper {
 public:
  // Hash function.
  template <class Key, class Value>
  size_t operator()(const std::pair<Key, Value>& pair) const {
    return fn_(pair.first);
  }
  template <class Key>
  size_t operator()(const Key& key) const {
    return fn_(key);
  }
  template <class Key, class Value>
  bool operator()(const std::pair<Key, Value>& a, const std::pair<Key, Value>& b) const {
    return fn_(a.first, b.first);
  }
  template <class Key, class Value, class Element>
  bool operator()(const std::pair<Key, Value>& a, const Element& element) const {
    return fn_(a.first, element);
  }

 private:
  Fn fn_;
};

template <class Key, class Value, class EmptyFn,
    class HashFn = std::hash<Key>, class Pred = std::equal_to<Key>,
    class Alloc = std::allocator<std::pair<Key, Value>>>
class HashMap : private HashSet<std::pair<Key, Value>,
                                EmptyFn,
                                HashMapWrapper<HashFn>,
                                HashMapWrapper<Pred>,
                                Alloc> {
 private:
  using Base = HashSet<std::pair<Key, Value>,
                       EmptyFn,
                       HashMapWrapper<HashFn>,
                       HashMapWrapper<Pred>,
                       Alloc>;

 public:
  using value_type = typename Base::value_type;
  using allocator_type = typename Base::allocator_type;
  using reference = typename Base::reference;
  using const_reference = typename Base::const_reference;
  using pointer = typename Base::pointer;
  using const_pointer = typename Base::const_pointer;
  using iterator = typename Base::iterator;
  using const_iterator = typename Base::const_iterator;
  using size_type = typename Base::size_type;
  using difference_type = typename Base::difference_type;

  HashMap() noexcept : Base() { }
  HashMap(const HashMap& other) : Base(other) {}
  HashMap(HashMap&& other) noexcept : Base(std::move(other)) {}

  explicit HashMap(const Alloc& alloc) noexcept
      : Base(alloc) { }

  HashMap& operator=(const HashMap& other) noexcept {
    static_cast<Base&>(*this) = other;
    return *this;
  }

  HashMap& operator=(HashMap&& other) noexcept {
    static_cast<Base&>(*this) = std::move(other);
    return *this;
  }

  using Base::Clear;
  using Base::Empty;
  using Base::Erase;
  using Base::Find;
  using Base::Insert;
  using Base::Size;

  // Compatibility with SafeMap

  using Base::begin;
  using Base::end;
  using Base::get_allocator;
  using Base::swap;

  bool empty() const {
    return Base::Empty();
  }

  size_type size() const {
    return Base::Size();
  }

  size_type count(const Key& key) const {
    return find(key) == end() ? 0 : 1;
  }

  template<typename V>
  iterator Put(const Key& k, V&& v) {
    // Make sure we're not adding a duplicate
    DCHECK(Base::Find(k) == Base::end());
    return Base::Insert(value_type(k, std::forward<V>(v)));
  }

  void clear() {
    Base::Clear();
  }

  template<typename K>
  iterator find(const K& key) {
    return Base::Find(key);
  }

  template<typename K>
  const_iterator find(const K& key) const {
    return Base::Find(key);
  }

  iterator erase(iterator it) {
    return Erase(it);
  }

  template<typename K>
  size_t erase(const K& key) {
    iterator it = Base::Find(key);
    if (it == Base::end()) {
      return 0;
    }
    Base::Erase(it);
    return 1;
  }

  bool operator==(const HashMap& other) const {
    return static_cast<const Base&>(*this) == other;
  }

  bool operator!=(const HashMap& other) const {
    return static_cast<const Base&>(*this) != other;
  }
};

template <class Key, class Value, class EmptyFn, class HashFn, class Pred, class Alloc>
void swap(HashMap<Key, Value, EmptyFn, HashFn, Pred, Alloc>& lhs,
          HashMap<Key, Value, EmptyFn, HashFn, Pred, Alloc>& rhs) noexcept {
  lhs.swap(rhs);
}

}  // namespace art

#endif  // ART_LIBARTBASE_BASE_HASH_MAP_H_
