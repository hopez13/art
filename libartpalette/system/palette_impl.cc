/*
 * Copyright (C) 2019 The Android Open Source Project
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

#define ATRACE_TAG ATRACE_TAG_DALVIK

#include "palette/palette.h"

#include <errno.h>
#ifdef __linux__
#include <sys/resource.h>
#endif  // __linux__
#include <sys/time.h>

#include <cutils/sched_policy.h>
#include <cutils/trace.h>

#ifdef ART_TARGET_ANDROID
#include <metricslogger/metrics_logger.h>
#endif

#include <utils/Thread.h>

namespace art {
namespace palette {

static constexpr int kPaletteVersion = 1;

// Managed thread definitions
static constexpr int kNormalManagedThreadPriority = 5;
#ifdef ART_TARGET_ANDROID
static constexpr int kMinManagedThreadPriority = 1;
static constexpr int kMaxManagedThreadPriority = 10;
static constexpr int kNumManagedThreadPriorities =
    kMaxManagedThreadPriority - kMinManagedThreadPriority + 1;
#endif  // ART_TARGET_ANDROID

}  // namespace palette
}  // namespace art

PaletteStatus PaletteGetVersion(int* version) {
  *version = art::palette::kPaletteVersion;
  return PaletteStatus::kOkay;
}

#ifdef ART_TARGET_ANDROID

enum PaletteStatus PaletteMetricsLogEvent(
    int32_t category,
    const char* package_name,
    const PaletteMetricsRecordTaggedData* tagged_data,
    int32_t tagged_data_count) {
  int metrics_category;
  switch (category) {
    case PaletteEventCategory::kHiddenApiAccess:
      metrics_category = android::metricslogger::ACTION_HIDDEN_API_ACCESSED;
      break;
    default:
      return PALETTE_INVALID_ARGUMENT;
  }

  android::metricslogger::ComplexEventLogger log_maker(metrics_category);
  if (package_name != nullptr) {
    log_maker.SetPackageName(package_name);
  }

  for (size_t i = 0; i < tagged_data_count) {
    switch (tagged_data[i].tag) {
      case PaletteEventTag::kHiddenApiAccessMethod:
        DCHECK_EQ(tagged_data[i].kind, PaletteEventTaggedDataKind::kInt32);
        tag = FIELD_HIDDEN_API_ACCESS_METHOD;
        switch (tagged_data[i].value.i32) {
          case PaletteEventCategoryHiddenApiAccess::kNone:
            log_maker.AddTaggedData(android::metricslogger::FIELD_HIDDEN_API_ACCESS_METHOD,
                                    android::metricslogger::ACCESS_METHOD_NONE);
            break;
          case PaletteEventCategoryHiddenApiAccess::kMethodViaReflection:
            log_maker.AddTaggedData(android::metricslogger::FIELD_HIDDEN_API_ACCESS_METHOD,
                                    android::metricslogger::ACCESS_METHOD_REFLECTION);
            break;
          case PaletteEventCategoryHiddenApiAccess::kMethodViaJNI:
            log_maker.AddTaggedData(android::metricslogger::FIELD_HIDDEN_API_ACCESS_METHOD,
                                    android::metricslogger::ACCESS_METHOD_JNI);
            break;
          case PaletteEventCategoryHiddenApiAccess::kMethodViaLinking:
            log_maker.AddTaggedData(android::metricslogger::FIELD_HIDDEN_API_ACCESS_METHOD,
                                    android::metricslogger::ACCESS_METHOD_LINKING);
            break;
          default:
            return PALETTE_INVALID_ARGUMENT;
        }
        break;
      case PaletteEventTag::kHiddenApiAccessDenied:
        DCHECK_EQ(tagged_data[i].kind, PaletteEventTaggedDataKind::kInt32);
        log_maker.AddTaggedData(FIELD_HIDDEN_API_ACCESS_DENIED, tagged_data[i].value.i32);
        break;
      case PaletteEventTag::kHiddenApiSignature:
        DCHECK_EQ(tagged_data[i].kind, PaletteEventTaggedDataKind::kString);
        log_maker.AddTaggedData(FIELD_HIDDEN_API_SIGNATURE, tagged_data[i].value.string);;
        break;
      default:
        return PALETTE_INVALID_ARGUMENT;
    }
  }

  log_maker.Record();
}

// Conversion map for "nice" values.
//
// We use Android thread priority constants to be consistent with the rest
// of the system.  In some cases adjacent entries may overlap.
//
static const int kNiceValues[art::palette::kNumManagedThreadPriorities] = {
  ANDROID_PRIORITY_LOWEST,                // 1 (MIN_PRIORITY)
  ANDROID_PRIORITY_BACKGROUND + 6,
  ANDROID_PRIORITY_BACKGROUND + 3,
  ANDROID_PRIORITY_BACKGROUND,
  ANDROID_PRIORITY_NORMAL,                // 5 (NORM_PRIORITY)
  ANDROID_PRIORITY_NORMAL - 2,
  ANDROID_PRIORITY_NORMAL - 4,
  ANDROID_PRIORITY_URGENT_DISPLAY + 3,
  ANDROID_PRIORITY_URGENT_DISPLAY + 2,
  ANDROID_PRIORITY_URGENT_DISPLAY         // 10 (MAX_PRIORITY)
};

enum PaletteStatus PaletteSchedSetPriority(int32_t tid, int32_t managed_priority) {
  if (managed_priority < art::palette::kMinManagedThreadPriority ||
      managed_priority > art::palette::kMaxManagedThreadPriority) {
      managed_priority = art::palette::kNormalManagedThreadPriority;
  }
  int newNice = kNiceValues[managed_priority - art::palette::kMinManagedThreadPriority];

  // TODO: b/18249098 The code below is broken. It uses getpriority() as a proxy for whether a
  // thread is already in the SP_FOREGROUND cgroup. This is not necessarily true for background
  // processes, where all threads are in the SP_BACKGROUND cgroup. This means that callers will
  // have to call setPriority twice to do what they want :
  //
  //     Thread.setPriority(Thread.MIN_PRIORITY);  // no-op wrt to cgroups
  //     Thread.setPriority(Thread.MAX_PRIORITY);  // will actually change cgroups.
  if (newNice >= ANDROID_PRIORITY_BACKGROUND) {
    set_sched_policy(tid, SP_BACKGROUND);
  } else if (getpriority(PRIO_PROCESS, tid) >= ANDROID_PRIORITY_BACKGROUND) {
    set_sched_policy(tid, SP_FOREGROUND);
  }

  if (setpriority(PRIO_PROCESS, tid, newNice) != 0) {
    return PaletteStatus::kCheckErrno;
  }
  return PaletteStatus::kOkay;
}

enum PaletteStatus PaletteSchedGetPriority(int32_t tid, int32_t* managed_priority) {
  errno = 0;
  int native_priority = getpriority(PRIO_PROCESS, tid);
  if (native_priority == -1 && errno != 0) {
    *managed_priority = art::palette::kNormalManagedThreadPriority;
    return PaletteStatus::kCheckErrno;
  }

  int p = art::palette::kMinManagedThreadPriority;
  while (p <= art::palette::kMaxManagedThreadPriority) {
    int index = p - art::palette::kMinManagedThreadPriority;
    if (native_priority >= kNiceValues[index]) {
      *managed_priority = p;
      return PaletteStatus::kOkay;
    }
    p = p + 1;
  }
  *managed_priority = art::palette::kMaxManagedThreadPriority;
  return PaletteStatus::kOkay;
}

enum PaletteStatus PaletteTraceEnabled(int32_t* enabled) {
  *enabled = (ATRACE_ENABLED() != 0) ? 1 : 0;
  return PaletteStatus::kOkay;
}

enum PaletteStatus PaletteTraceBegin(const char* name) {
  ATRACE_BEGIN(name);
  return PaletteStatus::kOkay;
}

enum PaletteStatus PaletteTraceEnd() {
  ATRACE_END();
  return PaletteStatus::kOkay;
}

enum PaletteStatus PaletteTraceIntegerValue(const char* name, int32_t value) {
  ATRACE_INT(name, value);
  return PaletteStatus::kOkay;
}

#else  // ART_TARGET_ANDROID

enum PaletteStatus PaletteMetricsLogEvent(
    int32_t,
    const char*,
    const PaletteMetricsRecordTaggedData*,
    int32_t) {
  return PaletteStatus::kNotSupported;
}

enum PaletteStatus PaletteSchedSetPriority(int32_t, int32_t) {
  return PaletteStatus::kOkay;
}

enum PaletteStatus PaletteSchedGetPriority(int32_t, int32_t* priority) {
  *priority = art::palette::kNormalManagedThreadPriority;
  return PaletteStatus::kOkay;
}

enum PaletteStatus PaletteTraceInit() {
  return PaletteStatus::kNotSupported;
}

enum PaletteStatus PaletteTraceEnabled(int32_t*) {
  return PaletteStatus::kNotSupported;
}

enum PaletteStatus PaletteTraceBegin(const char*) {
  return PaletteStatus::kNotSupported;
}

enum PaletteStatus PaletteTraceEnd() {
  return PaletteStatus::kNotSupported;
}

enum PaletteStatus PaletteTraceIntegerValue(const char*, int32_t) {
  return PaletteStatus::kNotSupported;
}

#endif  // ART_TARGET_ANDROID
