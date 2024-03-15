#ifndef ART_RUNTIME_GC_COLLECTOR_UTILS_PERF_HINT_H
#define ART_RUNTIME_GC_COLLECTOR_UTILS_PERF_HINT_H

namespace art {

class PerfHint {
 public:
   // set main thread scheduling attributes
   static void setSchedAttr(bool enabled, int tid);
};

}  // namespace art

#endif
