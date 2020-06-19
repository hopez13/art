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

#include "interpreter_cache.h"
#include "thread-inl.h"

namespace art {

static std::atomic<uint32_t> gHits;
static std::atomic<uint32_t> gUnnecessaryHits;
static std::atomic<uint32_t> gMisses;
static std::atomic<uint32_t> gAvoidableMisses;
static std::atomic<uint32_t> gAvoidableStores;

std::string DescribeInterpreterCacheUse() {
  std::ostringstream oss;
  oss << "hits: " << gHits.load(std::memory_order_relaxed)
      << " unnecessary: " << gUnnecessaryHits.load(std::memory_order_relaxed)
      << " misses: " << gMisses.load(std::memory_order_relaxed)
      << " avoidable: " << gAvoidableMisses.load(std::memory_order_relaxed)
      << " avoidable_stores: " << gAvoidableStores.load(std::memory_order_relaxed);
  return oss.str();
}

bool InterpreterCache::Get(const void* key, /* out */ size_t* value) {
  DCHECK(IsCalledFromOwningThread());
  size_t index = IndexOf(key);
  Entry& entry = data_[index];
  if (LIKELY(entry.first == key)) {
    *value = entry.second;
    gHits.fetch_add(1u, std::memory_order_relaxed);
    if (data2_active_[index]) {
      gUnnecessaryHits.fetch_add(1u, std::memory_order_relaxed);
    }
    return true;
  }
  gMisses.fetch_add(1u, std::memory_order_relaxed);
  if (data2_active_[index] && data2_[index].first == key) {
    gAvoidableMisses.fetch_add(1u, std::memory_order_relaxed);
  }
  return false;
}

void InterpreterCache::Set(const void* key, size_t value, bool avoidable) {
  DCHECK(IsCalledFromOwningThread());
  size_t index = IndexOf(key);
  data_[index] = Entry{key, value};
  if (!avoidable) {
    data2_active_[index] = false;
    data2_[index] = Entry{};
  } else {
    gAvoidableStores.fetch_add(1u, std::memory_order_relaxed);
    if (!data2_active_[index]) {
      data2_active_[index] = true;
      data2_[index] = data_[index];
    }
  }
}

void InterpreterCache::Clear(Thread* owning_thread) {
  DCHECK(owning_thread->GetInterpreterCache() == this);
  DCHECK(owning_thread == Thread::Current() || owning_thread->IsSuspended());
  data_.fill(Entry{});
  data2_.fill(Entry{});
  data2_active_.reset();
}

bool InterpreterCache::IsCalledFromOwningThread() {
  return Thread::Current()->GetInterpreterCache() == this;
}

}  // namespace art
