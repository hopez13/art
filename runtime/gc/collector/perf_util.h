
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
#include <android-base/properties.h>

namespace art {
namespace gc {
namespace collector {

static std::vector<int32_t> cpu_set_;
static std::string config_cpu_set;
static cpu_set_t default_mask;

class PerfUtil {

public:
static void SetTaskAffinity(int tid) {
    if (cpu_set_.empty()) {
        if (config_cpu_set.empty()) {
            //TODO:need get it from product config
            config_cpu_set = android::base::GetProperty("ro.vendor.heaptask-cpu-set", "0,1,2,3,4,5,6");
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

static void RestoreTaskAffinity(int tid) {
    if (!cpu_set_.empty()) {
        RestoreCpuAffinity(tid);
    }
 }

private:
static void SetCpuAffinity(const std::vector<int32_t>& cpu_list, int pid) {
#ifdef __linux__
  //Cache system default mask
  if(CPU_COUNT(&default_mask) == 0) {
     CPU_ZERO(&default_mask);
     if (sched_getaffinity(pid, sizeof(cpu_set_t), &default_mask) == -1) {
         LOG(INFO) << "Failed to get cuurent CPU affinity.";
     }
  }

  //Set new mask
  int cpu_count = sysconf(_SC_NPROCESSORS_CONF);
  cpu_set_t target_cpu_set;
  CPU_ZERO(&target_cpu_set);

  for (int32_t cpu : cpu_list) {
    if (cpu >= 0 && cpu < cpu_count) {
        CPU_SET(cpu, &target_cpu_set);
        LOG(INFO) << "set_cc_gc affinity: cpu " << cpu << "; thread id = " << pid;
    } else {
        LOG(WARNING) << "Invalid cpu :" << cpu
                     << " specified in --cpu-set argument (nprocessors =" << cpu_count << ")";
    }
  }

  if (sched_setaffinity(pid, sizeof(cpu_set_t), &target_cpu_set) == -1) {
     LOG(INFO) << "Failed to set CPU affinity.";
  }
#else
  LOG(WARNING) << "--cpu-set not supported on this platform.";
#endif  // __linux__
}

static void RestoreCpuAffinity(int pid) {
#ifdef __linux__
  if (sched_setaffinity(pid, sizeof(cpu_set_t), &default_mask) == -1) {
     LOG(INFO) << "Failed to set CPU affinity.";
  }
#else
  LOG(WARNING) << "--cpu-set not supported on this platform.";
#endif  // __linux__
}

};

}  // namespace collector
}  // namespace gc
}  // namespace art

#endif