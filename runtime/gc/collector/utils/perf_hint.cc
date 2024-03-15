#include "base/logging.h"
#include "perf_hint.h"

#include <android-base/properties.h>
#include <sys/syscall.h>
#include <sched.h>
#include <unistd.h>

namespace art {

  // Currently, there is no wrapper in bionic: b/183240349.
  //copy from external/cronet/base/threading/platform_thread_linux.cc
  struct sched_attr {
      uint32_t size;
      uint32_t sched_policy;
      uint64_t sched_flags;

      /* SCHED_NORMAL, SCHED_BATCH */
      int32_t sched_nice;

      /* SCHED_FIFO, SCHED_RR */
      uint32_t sched_priority;

      /* SCHED_DEADLINE */
      uint64_t sched_runtime;
      uint64_t sched_deadline;
      uint64_t sched_period;

      /* Utilization hints */
      uint32_t sched_util_min;
      uint32_t sched_util_max;
  };

  #if defined(__x86_64__)
  #define NR_sched_setattr 314
  #elif defined(__i386__)
  #define NR_sched_setattr 351
  #elif defined(__arm__)
  #define NR_sched_setattr 380
  #elif defined(__aarch64__)
  #define NR_sched_setattr 274
  #else
  #error "We don't have an NR_sched_setattr for this architecture."
  #endif

  #define SCHED_FLAG_KEEP_POLICY 0x08
  #define SCHED_FLAG_KEEP_PARAMS 0x10
  #define SCHED_FLAG_UTIL_CLAMP_MIN 0x20
  #define SCHED_FLAG_UTIL_CLAMP_MAX 0x40
  #define SCHED_FLAG_KEEP_ALL (SCHED_FLAG_KEEP_POLICY | SCHED_FLAG_KEEP_PARAMS)
  #define SCHED_FLAG_UTIL_CLAMP (SCHED_FLAG_UTIL_CLAMP_MIN | SCHED_FLAG_UTIL_CLAMP_MAX)

 int sched_setattr(int pid, struct sched_attr* attr, unsigned int flags) {
     return syscall(NR_sched_setattr, pid, attr, flags);
 }

 void PerfHint::setSchedAttr(bool enabled, int tid) {
    static unsigned int kUclampMax =
             android::base::GetUintProperty<unsigned int>(
             "ro.vendor.heaptask-cpu-uclamp-max", 0U);

    //TODO: Add for test, remove it in final version
    kUclampMax = 638;

    if (!kUclampMax) {
        // vendor uclamp max is 0 (not set), skip setting
        VLOG(heap) << "No config for heaptask uclamp, skip setting";
    }
#ifdef __linux__
    sched_attr attr = {};
    attr.size = sizeof(attr);
    attr.sched_flags = (SCHED_FLAG_KEEP_ALL | SCHED_FLAG_UTIL_CLAMP);
    attr.sched_util_min = 0;
    attr.sched_util_max = enabled ? kUclampMax : 0;

    int ret = sched_setattr(tid, &attr, 0);
    VLOG(heap) << "set_gc uclamp: max " << attr.sched_util_max << "; thread id = " << tid;
    if (ret) {
        int err = ret;
        VLOG(heap) << "sched_setattr failed for thread " << tid << " err " << err;
    }
 #else
     VLOG(heap) << "sched_setattr not supported on this platform.";
 #endif
 }
}  // namespace art
