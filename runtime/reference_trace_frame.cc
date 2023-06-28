#include "reference_trace_frame.h"
#include "android-base/file.h"
#include "android-base/logging.h"
#include "base/mutex.h"
#include "indirect_reference_table.h"
#include "native_stack_dump.h"
#include "reference_table.h"
#include "runtime_common.h"
#include "thread.h"
#include <array>
#include <cstdint>
#include <memory>
#include <ostream>
#include <vector>
#include "runtime-inl.h"

#if defined (__aarch64__)
static constexpr size_t TOP_FRAMES = 5;
static constexpr int RECORD_TRACE_THTESHOLD = 200;
static constexpr int RELEASE_UNUSED_INTERVAL = 200;
// static constexpr unsigned TAG_SHIFT = 56;
// static constexpr unsigned CHECK_SHIFT = 48;
// static constexpr unsigned UNTAG_SHIFT = 40;

namespace art {

ALWAYS_INLINE
static inline bool cmp(FrameInfo* a, FrameInfo* b) {
  if (!a || !b) return false;
  return a->GetRefs() > b->GetRefs();
}

bool FrameInfo::operator==(FrameInfo& other) {
  auto otherLrArray = other.GetLrArray();
  for (int i = BT_TRACE_COUNT - 1; i >= 0; i--) {
    if (mLrArray[i] == 0) continue;
    if (mLrArray[i] != otherLrArray[i]) {
      return false;
    }
  }
  if(other.GetClass() == GetClass() &&
     other.GetObjectCount() == GetObjectCount()) {
    return true;
  }
  return false;
}

bool FrameInfo::operator==(size_t* lrs) {
  for (int i = BT_TRACE_COUNT - 1; i >= 0; i--) {
    if (mLrArray[i] != lrs[i]) {
      return false;
    }
  }
  return true;
}

void FrameInfo::AddRefs() {
  mRefs = mRefs + 1;
}

void FrameInfo::RemoveRefs() {
  if (mRefs == 0) return;
  mRefs = mRefs - 1;
}

// Mutex for protecting Frame Maps and Frame-IDX Maps during concurrent access.
std::mutex IndirectReferenceStatistics::frame_mutex_;

IndirectReferenceStatistics::IndirectReferenceStatistics() {
}

void IndirectReferenceStatistics::Dump(std::ostream& os) const {
  DumpTopFrame(os);
}

void IndirectReferenceStatistics::DumpTopFrame(std::ostream& os) const {
  os << "Dump Top Frames: \n";
  std::vector<FrameInfo*> frames;

  {
    std::lock_guard<std::mutex> lock(frame_mutex_);
    for (auto iter = frame_map_.begin(); iter != frame_map_.end(); iter++) {
      for (auto v: *(iter->second)) {
        if (v && v->GetRefs() > RECORD_TRACE_THTESHOLD) {
          frames.emplace_back(v);
        }
      }
    }
  }

  std::sort(frames.begin(), frames.end(), cmp);
  size_t top = frames.size() < TOP_FRAMES ? frames.size() : TOP_FRAMES;
  for (size_t i = 0; i < top; i++) {
    os << " top: " << i << ", total: " << frames[i]->GetRefs() << "\n" <<*(frames[i]);
  }
}

IndirectReferenceStatistics::~IndirectReferenceStatistics() {
  std::lock_guard<std::mutex> lock(frame_mutex_);
  for (auto iter = frame_map_.begin(); iter != frame_map_.end(); iter++) {
    auto fv = iter->second;
    if (fv == nullptr) continue;
    for (size_t i = 0; i < fv->size(); i++) {
      if (fv->at(i)) {
        delete(fv->at(i));
        fv->at(i) = nullptr;
      }
    }
    if (fv) {
      delete(fv);
      iter->second = nullptr;
    }
  }
}

std::string IndirectReferenceStatistics::GetTraceDump() {
  auto self = Thread::Current();
  std::ostringstream oss;
  ArtMethod* method = self->GetCurrentMethod(nullptr,
                           /*check_suspended=*/ false,
                           /*abort_on_error=*/ false);
  DumpNativeStackSimplify(oss, GetTid(), "  native: ", method);
  self->DumpJavaStack(oss,
                  /*check_suspended=*/ false,
                  /*dump_locks=*/ false);
  //self->DumpStack(oss, true, nullptr, true);
  return oss.str();
}

void IndirectReferenceStatistics::AddLrs(size_t* lrs,
                                size_t idx, mirror::Class* classPtr, size_t objectCount)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  memcpy(lrs_, lrs, sizeof(size_t) * BT_TRACE_COUNT);
  RecordFrameinfo(idx, classPtr, objectCount);
}

void IndirectReferenceStatistics::ClearUnusedFrame() {
  release_times_ = 0;
  std::lock_guard<std::mutex> lock(frame_mutex_);
  for (auto iter = frame_map_.begin(); iter != frame_map_.end(); iter++) {
    auto v = iter->second;
    for (size_t index = 0; index < v->size(); index++) {
      auto fi = v->at(index);
      if (fi && fi->GetRefs() == 0) {
        delete(fi);
        v->at(index) = nullptr;
      }
    }
  }
}

void IndirectReferenceStatistics::RemoveLrs(size_t idx)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  {
    std::lock_guard<std::mutex> lock_(frame_mutex_);
    auto iter = frames_idx_maps_.find(idx);
    if (iter != frames_idx_maps_.end()) {
      auto fi = iter->second;
      if (fi != nullptr) {
        fi->RemoveRefs();
        release_times_++;
      }
    }
  }
  if (release_times_ > RELEASE_UNUSED_INTERVAL) {
    ClearUnusedFrame();
  }
}

void IndirectReferenceStatistics::RecordFrameinfo(size_t idx, mirror::Class* classPtr, size_t objectCount)
    REQUIRES_SHARED(Locks::mutator_lock_) {
      size_t vaild_last_addr = 0;
      for (int i = BT_TRACE_COUNT - 1; i >= 0; i--) {
        if (lrs_[i] != 0) {
          vaild_last_addr = lrs_[i];
          break;
        }
      }
      auto nfi = new FrameInfo(lrs_, classPtr, objectCount);
      std::lock_guard<std::mutex> lock(frame_mutex_);
      auto iter = frame_map_.find(vaild_last_addr);

      if (iter == frame_map_.end()) {
        // not Found
        auto fv = new std::vector<FrameInfo*>();
        fv->emplace_back(nfi);
        frame_map_[vaild_last_addr] = fv;
        frames_idx_maps_[idx] = nfi;
      } else {
        std::vector<FrameInfo*>* fv = iter->second;
        for (FrameInfo* fi : *fv) {
          if (fi && (*fi) == (*nfi)) {
            fi->AddRefs();
            if (fi->GetRefs() == RECORD_TRACE_THTESHOLD) {
              fi->SetTrace(GetTraceDump());
            }
            frames_idx_maps_[idx] = fi;
            delete nfi;
            return;
          }
        }
        fv->emplace_back(nfi);
        frames_idx_maps_[idx] = nfi;
      }
    }
}  // namespace art
#endif
