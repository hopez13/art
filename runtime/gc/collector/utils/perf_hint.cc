#include "base/logging.h"
#include "perf_hint.h"

#include <stdlib.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <sched.h>
#include <android-base/properties.h>
#include <vector>

namespace art {

 static std::vector<int32_t> cpu_set_;
 static std::string config_cpu_set;
 static cpu_set_t default_mask;

 struct sched_attr {
    uint32_t size;
    uint32_t sched_policy;
    uint64_t sched_flags;
    uint32_t sched_nice;
    uint32_t sched_priority;
    uint64_t sched_runtime;
    uint64_t sched_deadline;
    uint64_t sched_period;
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

 void PerfHint::SetUclamp(int32_t min, int32_t max, int tid) {
 #ifdef __linux__
     sched_attr attr = {};
     attr.size = sizeof(attr);
     attr.sched_flags = (SCHED_FLAG_KEEP_ALL | SCHED_FLAG_UTIL_CLAMP);
     attr.sched_util_min = min;
     attr.sched_util_max = max;

     int ret = sched_setattr(tid, &attr, 0);
     VLOG(heap) << "set_cc_gc uclamp: max " <<  max << "; thread id = " << tid;
     if (ret) {
         int err = ret;
         VLOG(heap) << "sched_setattr failed for thread " << tid << " err=" << err;
     }
 #else
     VLOG(heap) << "sched_setattr not supported on this platform.";
 #endif
 }

 void SetCpuAffinity(const std::vector<int32_t>& cpu_list, int pid) {
 #ifdef __linux__
  //Cache system default mask
  if(CPU_COUNT(&default_mask) == 0) {
     CPU_ZERO(&default_mask);
     if (sched_getaffinity(pid, sizeof(cpu_set_t), &default_mask) == -1) {
         VLOG(heap) << "Failed to get cuurent CPU affinity.";
     }
  }

  //Set new mask
  int cpu_count = sysconf(_SC_NPROCESSORS_CONF);
  cpu_set_t target_cpu_set;
  CPU_ZERO(&target_cpu_set);

  for (int32_t cpu : cpu_list) {
    if (cpu >= 0 && cpu < cpu_count) {
        CPU_SET(cpu, &target_cpu_set);
        VLOG(heap) << "set_cc_gc affinity: cpu " << cpu << "; thread id = " << pid;
    } else {
        VLOG(heap) << "Invalid cpu :" << cpu
                     << " specified in --cpu-set argument (nprocessors =" << cpu_count << ")";
    }
  }

  if (sched_setaffinity(pid, sizeof(cpu_set_t), &target_cpu_set) == -1) {
     VLOG(heap) << "Failed to set CPU affinity.";
  }
 #else
  VLOG(heap) << "--cpu-set not supported on this platform.";
 #endif  // __linux__
 }

 void PerfHint::SetTaskAffinity(int tid) {
    if (cpu_set_.empty()) {
        if (config_cpu_set.empty()) {
            //TODO:need get it from product config
            config_cpu_set = android::base::GetProperty("ro.vendor.heaptask-cpu-set", "0,1,2,3,4,5,6,7");
	    std::stringstream ss(config_cpu_set);
	    std::string token;
	    while (std::getline(ss, token, ',')) {
	         cpu_set_.push_back(std::stoi(token));
	    }
            SetCpuAffinity(cpu_set_, tid);
        }
     } else {
        SetCpuAffinity(cpu_set_, tid);
     }
 }

 void RestoreCpuAffinity(int pid) {
 #ifdef __linux__
  if (sched_setaffinity(pid, sizeof(cpu_set_t), &default_mask) == -1) {
     VLOG(heap) << "Failed to set CPU affinity.";
  }
 #else
  VLOG(heap) << "--cpu-set not supported on this platform.";
 #endif  // __linux__
 }

 void PerfHint::RestoreTaskAffinity(int tid) {
    if (!cpu_set_.empty()) {
        RestoreCpuAffinity(tid);
    }
 }

}  // namespace art

