/*
 * Copyright (C) 2017 The Android Open Source Project
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

#ifndef ART_RUNTIME_CALL_STACK_TRACKER_H_
#define ART_RUNTIME_CALL_STACK_TRACKER_H_

#include <iosfwd>
#include <set>
#include <string>
#include <unordered_map>

#include "base/macros.h"
#include "backtrace_helper.h"

namespace art {

class LOCKABLE Mutex;

class CallStackTracker {
 public:
  CallStackTracker();

  // Dump all of the stack traces in reverse sorted order of stack trace count.
  // NO_THREAD_SAFETY_ANALYSIS to avoid pulling in mutex.h
  void Dump(std::ostream& os) NO_THREAD_SAFETY_ANALYSIS;

  // Record a stack trace. Increments the count if the stack trace already exists.
  // NO_THREAD_SAFETY_ANALYSIS to avoid pulling in mutex.h
  void Record(std::string&& name, size_t skip_frames) NO_THREAD_SAFETY_ANALYSIS;

  static CallStackTracker* Current() {
    return instance_;
  }

  // Reset the globla callstack tracker instance.
  static void Reset(CallStackTracker* instance = nullptr);

  // Return true if we are tracking a dex location.
  bool TrackDexLocation(const std::string& dex_location) const;

  template <typename Container>
  void AddDexFiles(const Container& container) {
    // Dex files that we track.
    tracked_dex_files_.insert(container.begin(), container.end());
  }

 private:
  class CallStack {
   public:
    FixedSizeBacktrace</* max_frames */ 8> frames;
    std::string name;
    // pretty_backtrace is not part of the key, its just for dumping so that we get a nice
    // symbolized trace.
    std::string pretty_backtrace;

    CallStack() = default;
    CallStack(CallStack&& other) {
      frames = other.frames;
      name = std::move(other.name);
      pretty_backtrace = std::move(other.pretty_backtrace);
    }

    struct HashEquals {
      size_t operator()(const CallStack& key) const {
        size_t hash = 13;
        for (char c : key.name) {
          hash = hash * 31 + c;
        }
        return key.frames.Hash(hash);
      }

      bool operator()(const CallStack& a, const CallStack& b) const {
        return a.frames == b.frames && a.name == b.name;
      }
    };

    void Dump(std::ostream& os) const;
  };

  std::unique_ptr<Mutex> lock_;
  std::unordered_map<CallStack, size_t, CallStack::HashEquals, CallStack::HashEquals> call_stacks_;
  // Dex files that we track.
  std::set<std::string> tracked_dex_files_;
  // Avoid using unique_ptr to prevent global destructors.
  static CallStackTracker* instance_;
};


}  // namespace art

#endif  // ART_RUNTIME_CALL_STACK_TRACKER_H_
