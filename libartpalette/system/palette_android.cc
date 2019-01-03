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
#include <sys/resource.h>
#include <sys/time.h>

#include <android-base/logging.h>
#include <android-base/macros.h>
#include <cutils/sched_policy.h>
#include <cutils/trace.h>
#include <metricslogger/metrics_logger.h>
#include <utils/Thread.h>

#include "palette_system.h"

enum PaletteStatus PaletteGetVersion(int32_t* version) {
  *version = art::palette::kPaletteVersion;
  return PaletteStatus::kOkay;
}

enum PaletteStatus PaletteMetricsLogEvent(int32_t category,
                                          const char* package_name,
                                          const PaletteMetricsRecordTaggedData* tagged_data,
                                          int32_t tagged_data_count) {
  int metrics_category;
  switch (category) {
    case PaletteEventCategory::kHiddenApiAccess:
      metrics_category = android::metricslogger::ACTION_HIDDEN_API_ACCESSED;
      break;
    default:
      return PaletteStatus::kInvalidArgument;
  }

  android::metricslogger::ComplexEventLogger log_maker(metrics_category);
  if (package_name != nullptr) {
    log_maker.SetPackageName(package_name);
  }

  for (size_t i = 0; i < tagged_data_count; ++i) {
    switch (tagged_data[i].tag) {
      case PaletteEventTag::kHiddenApiAccessMethod:
        DCHECK_EQ(tagged_data[i].kind, PaletteEventTaggedDataKind::kInt32);
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
            return PaletteStatus::kInvalidArgument;
        }
        break;
      case PaletteEventTag::kHiddenApiAccessDenied:
        DCHECK_EQ(tagged_data[i].kind, PaletteEventTaggedDataKind::kInt32);
        log_maker.AddTaggedData(android::metricslogger::FIELD_HIDDEN_API_ACCESS_DENIED,
                                tagged_data[i].value.i32);
        break;
      case PaletteEventTag::kHiddenApiSignature:
        DCHECK_EQ(tagged_data[i].kind, PaletteEventTaggedDataKind::kString);
        log_maker.AddTaggedData(android::metricslogger::FIELD_HIDDEN_API_SIGNATURE,
                                tagged_data[i].value.c_str);
        break;
      default:
        return PaletteStatus::kInvalidArgument;
    }
  }

  log_maker.Record();
  return PaletteStatus::kOkay;
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
  int new_nice = kNiceValues[managed_priority - art::palette::kMinManagedThreadPriority];

  // TODO: b/18249098 The code below is broken. It uses getpriority() as a proxy for whether a
  // thread is already in the SP_FOREGROUND cgroup. This is not necessarily true for background
  // processes, where all threads are in the SP_BACKGROUND cgroup. This means that callers will
  // have to call setPriority twice to do what they want :
  //
  //     Thread.setPriority(Thread.MIN_PRIORITY);  // no-op wrt to cgroups
  //     Thread.setPriority(Thread.MAX_PRIORITY);  // will actually change cgroups.
  if (new_nice >= ANDROID_PRIORITY_BACKGROUND) {
    set_sched_policy(tid, SP_BACKGROUND);
  } else if (getpriority(PRIO_PROCESS, tid) >= ANDROID_PRIORITY_BACKGROUND) {
    set_sched_policy(tid, SP_FOREGROUND);
  }

  if (setpriority(PRIO_PROCESS, tid, new_nice) != 0) {
    return PaletteStatus::kCheckErrno;
  }
  return PaletteStatus::kOkay;
}

enum PaletteStatus PaletteSchedGetPriority(int32_t tid, /*out*/int32_t* managed_priority) {
  errno = 0;
  int native_priority = getpriority(PRIO_PROCESS, tid);
  if (native_priority == -1 && errno != 0) {
    *managed_priority = art::palette::kNormalManagedThreadPriority;
    return PaletteStatus::kCheckErrno;
  }

  for (int p = art::palette::kMinManagedThreadPriority;
       p <= art::palette::kMaxManagedThreadPriority;
       p = p + 1) {
    int index = p - art::palette::kMinManagedThreadPriority;
    if (native_priority >= kNiceValues[index]) {
      *managed_priority = p;
      return PaletteStatus::kOkay;
    }
  }
  *managed_priority = art::palette::kMaxManagedThreadPriority;
  return PaletteStatus::kOkay;
}

enum PaletteStatus PaletteTraceEnabled(/*out*/int32_t* enabled) {
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
