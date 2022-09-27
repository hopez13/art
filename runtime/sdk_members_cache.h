#ifndef ART_RUNTIME_SDK_MEMBERS_CACHE_H_
#define ART_RUNTIME_SDK_MEMBERS_CACHE_H_

#include "base/hash_set.h"
#include "base/locks.h"
#include "dex/dex_file_reference.h"
#include "dex/method_reference.h"

namespace art {

class ArtField;
class ArtMethod;

class SdkMembersCache {
 public:
  SdkMembersCache() {}

  static size_t ComputeSdkFieldHash(ArtField* field)
      REQUIRES_SHARED(Locks::mutator_lock_);
  static size_t ComputeSdkMethodHash(ArtMethod* method)
      REQUIRES_SHARED(Locks::mutator_lock_);

  void InsertField(ArtField* field) { sdk_fields_set_.insert(field); }
  void InsertMethod(ArtMethod* method) { sdk_methods_set_.insert(method); }

  ArtField* FindField(DexFileReference ref, uint32_t hash) const;
  ArtMethod* FindMethod(MethodReference ref, uint32_t hash) const;

  void Dump() const NO_THREAD_SAFETY_ANALYSIS {
    LOG(ERROR) << "NGEO " << sdk_fields_set_.TotalProbeDistance();
    LOG(ERROR) << "NGEO " << sdk_fields_set_.CalculateLoadFactor();

    LOG(ERROR) << "NGEO " << sdk_methods_set_.TotalProbeDistance();
    LOG(ERROR) << "NGEO " << sdk_methods_set_.CalculateLoadFactor();
  }

 private:
  class SdkFieldHash {
   public:
    SdkFieldHash() {}

    // NO_THREAD_SAFETY_ANALYSIS: This is called from unannotated `HashSet<>` functions.
    size_t operator()(ArtField* field) const NO_THREAD_SAFETY_ANALYSIS {
      return ComputeSdkFieldHash(field);
    }

    size_t operator()(DexFileReference ref) const;
  };

  class SdkFieldEqual {
   public:
    SdkFieldEqual() REQUIRES_SHARED(Locks::mutator_lock_) {}

    // NO_THREAD_SAFETY_ANALYSIS: This is called from unannotated `HashSet<>` functions.
    bool operator()(ArtField* field, DexFileReference reference) const NO_THREAD_SAFETY_ANALYSIS;

    bool operator()(ArtField* lhs, ArtField* rhs) const NO_THREAD_SAFETY_ANALYSIS {
      return lhs == rhs;
    }
  };

  class SdkMethodHash {
   public:
    SdkMethodHash() {}

    // NO_THREAD_SAFETY_ANALYSIS: This is called from unannotated `HashSet<>` functions.
    size_t operator()(ArtMethod* method) const NO_THREAD_SAFETY_ANALYSIS {
      return ComputeSdkMethodHash(method);
    }

    size_t operator()(MethodReference ref) const;
  };

  class SdkMethodEqual {
   public:
    SdkMethodEqual() {}

    // NO_THREAD_SAFETY_ANALYSIS: This is called from unannotated `HashSet<>` functions.
    bool operator()(ArtMethod* method, MethodReference reference) const NO_THREAD_SAFETY_ANALYSIS;

    bool operator()(ArtMethod* lhs, ArtMethod* rhs) const NO_THREAD_SAFETY_ANALYSIS {
      return lhs == rhs;
    }
  };

  using SdkFieldsSet = HashSet<ArtField*, DefaultEmptyFn<ArtField*>, SdkFieldHash, SdkFieldEqual>;
  using SdkMethodsSet = HashSet<ArtMethod*, DefaultEmptyFn<ArtMethod*>, SdkMethodHash, SdkMethodEqual>;

  SdkFieldsSet sdk_fields_set_;
  SdkMethodsSet sdk_methods_set_;
};

}

#endif  // ANDROID_STANDALONE9_ART_RUNTIME_SDK_MEMBERS_CACHE_H_
