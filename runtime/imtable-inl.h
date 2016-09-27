/*
 * Copyright (C) 2016 The Android Open Source Project
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

#ifndef ART_RUNTIME_IMTABLE_INL_H_
#define ART_RUNTIME_IMTABLE_INL_H_

#include "imtable.h"

#include "art_method-inl.h"
#include "dex_file.h"
#include "utf.h"

namespace art {

static constexpr bool kImTableHashUseName = true;
static constexpr bool kImTableHashUseCoefficients = true;

// Magic configuration that minimizes some common runtime calls.
static constexpr uint32_t kImTableHashCoefficient = 765445;

inline uint32_t ImTable::GetBaseImtHash(ArtMethod* method) REQUIRES_SHARED(Locks::mutator_lock_) {
  if (kImTableHashUseName) {
    if (method->IsProxyMethod()) {
      return 0;
    }

    // While it would be simplest to use PrettyMethod to get a string that is fully qualified and
    // unique, the string allocations and pretty-printing of types are overhead. Instead, break up
    // the hash.
    uint32_t hash = 0;

    hash = ComputeModifiedUtf8Hash(method->GetDeclaringClassDescriptor());

    const DexFile* dex_file = method->GetDexFile();
    const DexFile::MethodId& method_id = dex_file->GetMethodId(method->GetDexMethodIndex());

    // First use the class descriptor.
    hash = ComputeModifiedUtf8Hash(dex_file->GetMethodDeclaringClassDescriptor(method_id));

    // Mix in the method name.
    hash = 31 * hash + ComputeModifiedUtf8Hash(dex_file->GetMethodName(method_id));

    const DexFile::ProtoId& proto_id = dex_file->GetMethodPrototype(method_id);

    // Mix in the return type.
    hash = 31 * hash + ComputeModifiedUtf8Hash(
        dex_file->GetTypeDescriptor(dex_file->GetTypeId(proto_id.return_type_idx_)));

    // Mix in the argument types.
    // Note: we could consider just using the shorty. This would be faster, at the price of
    //       potential collisions.
    const DexFile::TypeList* param_types = dex_file->GetProtoParameters(proto_id);
    if (param_types != nullptr) {
      for (size_t i = 0; i != param_types->Size(); ++i) {
        const DexFile::TypeItem& type = param_types->GetTypeItem(i);
        hash = 31 * hash + ComputeModifiedUtf8Hash(
            dex_file->GetTypeDescriptor(dex_file->GetTypeId(type.type_idx_)));
      }
    }

    return hash;
  } else {
    return method->GetDexMethodIndex();
  }
}

inline uint32_t ImTable::GetImtIndex(ArtMethod* method) {
  if (!kImTableHashUseCoefficients) {
    return GetBaseImtHash(method) % ImTable::kSize;
  } else {
    return (kImTableHashCoefficient * GetBaseImtHash(method)) % ImTable::kSize;
  }
}

}  // namespace art

#endif  // ART_RUNTIME_IMTABLE_INL_H_

