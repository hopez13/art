#ifndef ART_RUNTIME_GC_COLLECTOR_UTILS_PERF_HINT_H
#define ART_RUNTIME_GC_COLLECTOR_UTILS_PERF_HINT_H

#include "base/logging.h"

#include <stdlib.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <sched.h>
#include <android-base/properties.h>

namespace art {

class PerfHint {
 public:
 //TODO: All value should read from product config
 static void SetCpuPolicy(int tid) {
     //Set CPU set
     VLOG(heap) << "set cpu set to 0-6(?).";
     SetTaskAffinity(tid);

     //Set uclamp,set 638 to match DX3 2.4ghz
     VLOG(heap) << "set uclamp max to 638.";
     SetUclamp(0, 638, tid);
 }

 static void RestoreCpuPolicy(int tid) {
    //Restore CPU set
    VLOG(heap) << "restore cpu set to 0-8(?).";
    RestoreTaskAffinity(tid);

    //Restore uclamp, set 1024 to restore default
    VLOG(heap) << "restore uclamp max to 1024.";
    SetUclamp(0, 1024, tid);
 }

 private:
 //Used to set thread CPU frequency celling.
 static void SetUclamp(int32_t min, int32_t max, int tid);

 //Used to set thread CPU set
 static void SetTaskAffinity(int tid);

 //Used to reset thread CPU set
 static void RestoreTaskAffinity(int tid);
};

}  // namespace art

#endif
