/*
 * Copyright (C) 2018 The Android Open Source Project
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

#ifndef ART_LIBARTBASE_BASE_SAFE_BI_MAP_H_
#define ART_LIBARTBASE_BASE_SAFE_BI_MAP_H_

#include <map>
#include <memory>
#include <type_traits>

#include <android-base/logging.h>

#include "safe_map.h"

namespace art {

// A basic bi-directional map. It is limited to having trivial contents to simplify implementation.
// TODO Remove triviality requirement.
template <typename K,
          typename V,
          typename ComparatorLeft = std::less<K>,
          typename ComparatorRight = std::less<V>,
          typename AllocatorLeft = std::allocator<std::pair<const K, V>>,
          typename AllocatorRight = std::allocator<std::pair<const V, K>>,
          typename = typename std::enable_if<
                std::is_trivial<K>::value &&
                std::is_trivial<V>::value &&
                std::is_trivially_destructible<K>::value &&
                std::is_trivially_destructible<V>::value>::type>
class SafeBiMap {
 private:
  typedef SafeBiMap<K, V, ComparatorLeft, ComparatorRight, AllocatorLeft, AllocatorRight> Self;
  typedef typename ::art::SafeMap<K, V, ComparatorLeft, AllocatorLeft> left_type;
  typedef typename ::art::SafeMap<V, K, ComparatorRight, AllocatorRight> right_type;

  typedef typename right_type::iterator right_iterator;
  typedef typename right_type::const_iterator right_const_iterator;

 public:
  typedef typename left_type::key_compare key_compare;
  typedef typename right_type::key_compare value_compare;
  typedef typename left_type::allocator_type allocator_left_type;
  typedef typename right_type::allocator_type allocator_right_type;
  typedef typename left_type::iterator iterator;
  typedef typename left_type::const_iterator const_iterator;
  typedef typename left_type::size_type size_type;
  typedef typename left_type::key_type key_type;
  typedef typename left_type::value_type value_type;

  SafeBiMap() = default;
  SafeBiMap(const SafeBiMap&) = default;
  SafeBiMap(SafeBiMap&&) = default;
  SafeBiMap(const key_compare& cmp_left,
            const value_compare& cmp_right,
            const allocator_left_type& allocator_left = allocator_left_type(),
            const allocator_right_type& allocator_right = allocator_right_type())
    : kv_map_(cmp_left, allocator_left), vk_map_(cmp_right, allocator_right) {
  }

  Self& operator=(const Self& rhs) {
    vk_map_ = rhs.vk_map_;
    kv_map_ = rhs.kv_map_;
    return *this;
  }

  allocator_left_type get_left_allocator() const { return kv_map_.get_allocator(); }
  allocator_right_type get_right_allocator() const { return vk_map_.get_allocator(); }
  key_compare key_comp() const { return kv_map_.key_comp(); }
  value_compare value_comp() const { return vk_map_.key_comp(); }

  iterator begin() { return kv_map_.begin(); }
  const_iterator begin() const { return kv_map_.begin(); }
  iterator end() { return kv_map_.end(); }
  const_iterator end() const { return kv_map_.end(); }

  bool empty() const { return kv_map_.empty(); }
  size_type size() const { return kv_map_.size(); }

  void swap(Self& other) {
    kv_map_.swap(other.kv_map_);
    vk_map_.swap(other.vk_map_);
  }

  void clear() {
    kv_map_.clear();
    vk_map_.clear();
  }

  iterator erase(iterator it) {
    vk_map_.erase(vk_map_.find(it->second));
    return kv_map_.erase(it);
  }

  size_type erase(const K& k) { return this->erase_left(k); }
  size_type erase_left(const K& k) {
    auto it = find_left(k);
    if (it != end()) {
      erase(it);
      return 1u;
    } else {
      return 0u;
    }
  }

  size_type erase_right(const V& v) {
    auto it = find_right(v);
    if (it != end()) {
      erase(it);
      return 1u;
    } else {
      return 0u;
    }
  }

  iterator find(const K& k) { return find_left(k); }
  iterator find_left(const K& k) { return kv_map_.find(k); }
  const_iterator find_left(const K& k) const { return kv_map_.find(k); }
  iterator find_right(const V& v) { return RightToLeft(vk_map_.find(v)); }
  const_iterator find_right(const V& v) const { return RightToLeft(vk_map_.find(v)); }

  iterator lower_bound(const K& k) { return lower_bound_left(k); }
  const_iterator lower_bound(const K& k) const { return lower_bound_left(k); }
  iterator lower_bound_left(const K& k) { return kv_map_.lower_bound(k); }
  const_iterator lower_bound_left(const K& k) const { return kv_map_.lower_bound(k); }
  iterator lower_bound_right(const V& v) { return RightToLeft(vk_map_.lower_bound(v)); }
  const_iterator lower_bound_right(const V& v) const { return RightToLeft(vk_map_.lower_bound(v)); }

  iterator upper_bound(const K& k) { return upper_bound_left(k); }
  const_iterator upper_bound(const K& k) const { return upper_bound_left(k); }
  iterator upper_bound_left(const K& k) { return kv_map_.upper_bound(k); }
  const_iterator upper_bound_left(const K& k) const { return kv_map_.upper_bound(k); }
  iterator upper_bound_right(const V& v) { return RightToLeft(vk_map_.upper_bound(v)); }
  const_iterator upper_bound_right(const V& v) const { return RightToLeft(vk_map_.upper_bound(v)); }

  size_type count(const K& k) const { return count_left(k); }
  size_type count_left(const K& k) const { return vk_map_.count(k); }
  size_type count_right(const V& v) const { return kv_map_.count(v); }

  V Get(const K& k) const { return GetLeft(k); }
  // Note that unlike std::map's operator[], this doesn't return a reference to the value.
  V GetLeft(const K& k) const {
    const_iterator it = kv_map_.find(k);
    DCHECK(it != kv_map_.end());
    return it->second;
  }

  // Note that unlike std::map's operator[], this doesn't return a reference to the value.
  K GetRight(const V& v) const {
    const_iterator it = vk_map_.find(v);
    DCHECK(it != vk_map_.end());
    return it->second;
  }

  // Used to insert a new mapping.
  iterator Put(const K& k, const V& v) {
    iterator result = kv_map_.Put(k, v);
    vk_map_.Put(v, k);
    return result;
  }

  // Used to insert a new mapping or overwrite an existing mapping. Note that if the value type
  // of this container is a pointer, any overwritten pointer will be lost and if this container
  // was the owner, you have a leak. Returns iterator pointing to the new or overwritten entry.
  iterator Overwrite(const K& k, const V& v) {
    erase_left(k);
    erase_right(v);
    return Put(k, v);
  }

  template <typename CreateFn>
  V GetOrCreateLeft(const K& k, CreateFn create) {
    static_assert(std::is_same<V, typename std::result_of<CreateFn()>::type>::value,
                  "Argument `create` should return a value of type V.");
    auto lb = lower_bound_left(k);
    if (lb != end() && !key_comp()(k, lb->first)) {
      return lb->second;
    }
    auto it = Put(k, create());
    return it->second;
  }

  template <typename CreateFn>
  V GetOrCreate(const K& k, CreateFn create) { return GetOrCreateLeft(k, create); }

  template <typename CreateFn>
  K GetOrCreateRight(const V& v, CreateFn create) {
    static_assert(std::is_same<K, typename std::result_of<CreateFn()>::type>::value,
                  "Argument `create` should return a value of type K.");
    auto lb = vk_map_.lower_bound(v);
    if (lb != end() && !value_comp()(v, lb->first)) {
      return lb->second;
    }
    auto it = Put(create(), v);
    return it->second;
  }

  iterator FindOrAdd(const K& k, const V& v) {
    iterator it = find_left(k);
    return it == end() ? Put(k, v) : it;
  }

  iterator FindOrAddLeft(const K& k) {
    iterator it = find_left(k);
    return it == end() ? Put(k, V()) : it;
  }

  iterator FindOrAddRight(const V& v) {
    iterator it = find_right(v);
    return it == end() ? Put(K(), v) : it;
  }

  bool Equals(const Self& rhs) const {
    return kv_map_ == rhs.kv_map_;
  }

 private:
  iterator RightToLeft(right_iterator ri) {
    if (ri == vk_map_.end()) {
      return kv_map_.end();
    } else if (ri == vk_map_.begin()) {
      return kv_map_.begin();
    } else {
      return kv_map_.find(ri->second);
    }
  }

  const_iterator RightToLeft(right_const_iterator ri) const {
    if (ri == vk_map_.cend()) {
      return kv_map_.cend();
    } else if (ri == vk_map_.cbegin()) {
      return kv_map_.cbegin();
    } else {
      return kv_map_.find(ri->second);
    }
  }

  SafeMap<K, V, ComparatorLeft, AllocatorLeft> kv_map_;
  SafeMap<V, K, ComparatorRight, AllocatorRight> vk_map_;
};


template <typename K,
          typename V,
          typename ComparatorLeft = std::less<K>,
          typename ComparatorRight = std::less<V>,
          typename AllocatorLeft = std::allocator<std::pair<const K, V>>,
          typename AllocatorRight = std::allocator<std::pair<const V, K>>,
          typename = typename std::enable_if<
                std::is_trivial<K>::value &&
                std::is_trivial<V>::value &&
                std::is_trivially_destructible<K>::value &&
                std::is_trivially_destructible<V>::value>::type>
bool operator==(const SafeBiMap<K,
                                V,
                                ComparatorLeft,
                                ComparatorRight,
                                AllocatorLeft,
                                AllocatorRight>& lhs,
                const SafeBiMap<K,
                                V,
                                ComparatorLeft,
                                ComparatorRight,
                                AllocatorLeft,
                                AllocatorRight>& rhs) {
  return lhs.Equals(rhs);
}

template <typename K,
          typename V,
          typename ComparatorLeft = std::less<K>,
          typename ComparatorRight = std::less<V>,
          typename AllocatorLeft = std::allocator<std::pair<const K, V>>,
          typename AllocatorRight = std::allocator<std::pair<const V, K>>,
          typename = typename std::enable_if<
                std::is_trivial<K>::value &&
                std::is_trivial<V>::value &&
                std::is_trivially_destructible<K>::value &&
                std::is_trivially_destructible<V>::value>::type>
bool operator!=(const SafeBiMap<K,
                                V,
                                ComparatorLeft,
                                ComparatorRight,
                                AllocatorLeft,
                                AllocatorRight>& lhs,
                const SafeBiMap<K,
                                V,
                                ComparatorLeft,
                                ComparatorRight,
                                AllocatorLeft,
                                AllocatorRight>& rhs) {
  return !(lhs == rhs);
}


}  // namespace art
#endif  // ART_LIBARTBASE_BASE_SAFE_BI_MAP_H_
