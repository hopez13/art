#ifndef ART_RUNTIME_REFERENCE_TRACE_FRAME_H_
#define ART_RUNTIME_REFERENCE_TRACE_FRAME_H_

#include <cstddef>
#include <cstdint>
#include <ostream>
#include <string>
#include <android-base/stringprintf.h>
#include <map>
#include "base/macros.h"
#include <atomic>
#include "base/atomic.h"
#include "base/locks.h"
#include "mirror/object.h"
#include "obj_ptr.h"
#include "gc_root.h"
#include "mirror/class-inl.h"
#include "mirror/class.h"

#if defined (__aarch64__)
static constexpr size_t BT_TRACE_COUNT = 12;
namespace art {
class FrameInfo {
  public:
    FrameInfo(const size_t* lrs, mirror::Class* classPtr, size_t objectCount){
      memcpy(mLrArray, lrs, sizeof(size_t)*BT_TRACE_COUNT);
      mRefs = 1;
      mClassPtr = classPtr;
      mObjectCount = objectCount;
    };

    bool operator==(FrameInfo& other);
    bool operator==(size_t* lrs);

    friend std::ostream& operator<<(std::ostream& os, FrameInfo& info) {
      std::string dump;
      std::string traceString = info.GetTrace();
      if(!traceString.empty()) {
        dump += traceString;
      }
      return os << dump;
    }

    size_t* GetLrArray() {
      return mLrArray;
    }

    void AddRefs();
    void RemoveRefs();
    size_t GetRefs() const { return mRefs; }
    mirror::Class* GetClass() const { return mClassPtr; }
    size_t GetObjectCount() const { return mObjectCount; }
    void SetTrace(std::string traceDump) {mTraceDump = traceDump;}
    std::string GetTrace() const {return mTraceDump;}
  private:
    size_t mLrArray[BT_TRACE_COUNT];
    size_t mRefs;
    mirror::Class* mClassPtr;
    size_t mObjectCount;
    std::string mTraceDump;
};

class IndirectReferenceStatistics {
  public:
    IndirectReferenceStatistics();
    ~IndirectReferenceStatistics();
    void AddLrs(size_t* lrs, size_t idx,
                mirror::Class* classPtr, size_t objectCount);
    void Dump(std::ostream& os) const;
    void DumpTopFrame(std::ostream& os) const;
    void RemoveLrs(size_t idx);
  private:
    size_t lrs_[BT_TRACE_COUNT];
    std::string GetTraceDump()
      REQUIRES_SHARED(Locks::mutator_lock_);
    void RecordFrameinfo(size_t idx, mirror::Class* classPtr, size_t objectCount)
      REQUIRES_SHARED(Locks::mutator_lock_);
    void ClearUnusedFrame();

    // Mutexes for protecting Frame Maps and Frame-IDX Maps during concurrent access.
    static std::mutex frame_mutex_;


    size_t release_times_ = 0;
    std::map<size_t, FrameInfo*> frames_idx_maps_ GUARDED_BY(frame_mutex_);
    std::map<size_t, std::vector<FrameInfo*>*> frame_map_ GUARDED_BY(frame_mutex_);
};
}  // namespace art
#endif

#endif
