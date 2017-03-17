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

#ifndef ART_METHOD_VERIFIER_STATS_H_
#define ART_METHOD_VERIFIER_STATS_H_

#include <atomic>
#include <iomanip>
#include <string>
#include <type_traits>

#include "atomic.h"

namespace art {

enum MethodVerifierStat {
  kVerifyErrorBadClassHard = 0,
  kVerifyErrorBadClassSoft,
  kVerifyErrorNoClass,
  kVerifyErrorNoField,
  kVerifyErrorNoMethod,
  kVerifyErrorAccessClass,
  kVerifyErrorAccessField,
  kVerifyErrorAccessMethod,
  kVerifyErrorClassChange,
  kVerifyErrorInstantiation,
  kVerifyErrorForceInterpreter,
  kVerifyErrorLocking,
  kLastStat
};

class MethodVerifierStats {
 public:
  MethodVerifierStats () {
    // The std::atomic<> default constructor leaves values uninitialized,
    // so initialize them now.
    Reset();
  }

  void RecordStat(MethodVerifierStat verifierStat, uint32_t count = 1) {
    verifier_stats_[error] += count;
  }

  void Log() const {
    if (!kIsDebugBuild) {
      // Log only in debug builds or if the compiler is verbose.
      return;
    }

    for (size_t i = 0; i < kLastStat; i++) {
      if (verifier_stats_[i] != 0) {
        LOG(INFO) << PrintMethodVerifierStat(static_cast<MethodVerifierStat>(i)) << ": "
                  << verifier_stats_[i];
      }
    }
  }

  void AddTo(MethodVerifierStats* other_stats) {
    for (size_t i = 0; i != kLastStat; ++i) {
      uint32_t count = verifier_stats_[i];
      if (count != 0) {
        other_stats->RecordStat(static_cast<MethodVerifierStat>(i), count);
      }
    }
  }

  void Reset() {
    for (size_t i = 0; i != kLastStat; ++i) {
      verifier_stats_[i] = 0u;
    }
  }

 private:
  std::string PrintMethodVerifierStat(MethodVerifierStat stat) const {
    std::string name;
    switch (stat) {
      case kVerifyErrorBadClassHard : name = "VerifyErrorBadClassHard"; break;
      case kVerifyErrorBadClassSoft : name = "VerifyErrorBadClassSoft"; break;
      case kVerifyErrorNoClass : name = "VerifyErrorNoClass"; break;
      case kVerifyErrorNoField : name = "VerifyErrorNoField"; break;
      case kVerifyErrorNoMethod : name = "VerifyErrorNoMethod"; break;
      case kVerifyErrorAccessClass : name = "VerifyErrorAccessClass"; break;
      case kVerifyErrorAccessField : name = "VerifyErrorAccessField"; break;
      case kVerifyErrorAccessMethod : name = "VerifyErrorAccessMethod"; break;
      case kVerifyErrorClassChange : name = "VerifyErrorClassChange"; break;
      case kVerifyErrorInstantiation : name = "VerifyErrorInstantiation"; break;
      case kVerifyErrorForceInterpreter : name = "VerifyErrorForceInterpreter"; break;
      case kVerifyErrorLocking : name = "VerifyErrorLocking"; break;

      case kLastStat:
        LOG(FATAL) << "invalid stat "
            << static_cast<std::underlying_type<MethodVerifierStat>::type>(stat);
        UNREACHABLE();
    }
    return "OptStat#" + name;
  }

  std::atomic<uint32_t> verifier_stats_[kLastStat];

  DISALLOW_COPY_AND_ASSIGN(MethodVerifierStats);
};

}  // namespace art

#endif  // ART_METHOD_VERIFIER_STATS_H_
