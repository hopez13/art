/*
 * Copyright (C) 2011 The Android Open Source Project
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

#ifndef ART_RUNTIME_GC_COLLECTOR_UCLAMP_H_
#define ART_RUNTIME_GC_COLLECTOR_UCLAMP_H_

#include <stdlib.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <sched.h>

namespace art {
namespace gc {
namespace collector {

class PerfUtil {

public:
 //TODO: All value should read from product config
 static void setUclampMax(int tid) {
    setUclamp(0, 638, tid);//frequency point is middle-core 2.4GHZ
    ScopedTrace trace(android::base::StringPrintf("set_uclamp %d ", 638));
 }

 static void restoreUclampMax(int tid) {
    setUclamp(0, 1024, tid);//frequency point 1024 is restore default
 }

 static void setCpuAffinity(int tid) {
    setCpuAffinity(6, tid);//core end 6 which contains little/middle cores
 }

 static void restoreCpuAffinity(int tid) {
    setCpuAffinity(7, tid);//core end 6 which contains all cores
 }

private:
 struct sched_attr {
    __u32 size;
    __u32 sched_policy;
    __u64 sched_flags;
    __s32 sched_nice;
    __u32 sched_priority;
    __u64 sched_runtime;
    __u64 sched_deadline;
    __u64 sched_period;
    __u32 sched_util_min;
    __u32 sched_util_max;
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

 static int sched_setattr(int pid, struct sched_attr* attr, unsigned int flags) {
     return syscall(NR_sched_setattr, pid, attr, flags);
 }

 static void setUclamp(int32_t min, int32_t max, int tid) {
 #ifdef __linux__
     sched_attr attr = {};
     attr.size = sizeof(attr);
     attr.sched_flags = (SCHED_FLAG_KEEP_ALL | SCHED_FLAG_UTIL_CLAMP);
     attr.sched_util_min = min;
     attr.sched_util_max = max;

     int ret = sched_setattr(tid, &attr, 0);
     LOG(INFO) << "set_cc_gc uclamp: max " <<  max << "; thread id = " << tid;
     if (ret) {
         int err = ret;
         LOG(ERROR) << "sched_setattr failed for thread " << tid << " err=" << err;
     }
 #else
 #endif
 }

 static void setCpuAffinity(int cend, int pid) {
 #ifdef __linux__
   cpu_set_t target_cpu_set;
   CPU_ZERO(&target_cpu_set);

   int core_begin = 0;
   int core_end = cend;

   for(int i = core_begin; i <= core_end; ++i) {
       CPU_SET(i, &target_cpu_set);
   }

   LOG(INFO) << "set_cc_gc affinity: core_end " <<  core_end << "; thread id = " << tid;
   if (sched_setaffinity(pid, sizeof(target_cpu_set), &target_cpu_set) == -1) {
       LOG(WARNING) << "Failed to set CPU affinity.";
   }
 #else
   LOG(WARNING) << "--cpu-set not supported on this platform.";
 #endif
 }

};

}  // namespace collector
}  // namespace gc
}  // namespace art

#endif
