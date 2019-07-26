/*
 * Copyright (C) 2019 The Android Open Source Project
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

#include "api_checker.h"

#include "art_method-inl.h"
#include "base/utils.h"
#include "dex/art_dex_file_loader.h"
#include "mirror/class-inl.h"

namespace art {

ApiChecker::ApiChecker() {}

ApiChecker* ApiChecker::Create(
    const std::string& api_classpath, std::string* error_msg) {
  std::vector<std::string> classpath_elems;
  Split(api_classpath, ':', &classpath_elems);

  ArtDexFileLoader dex_loader;

  std::unique_ptr<ApiChecker> api_checker(new ApiChecker());
  for (const std::string& path : classpath_elems) {
    if (!dex_loader.Open(path.c_str(),
                         path,
                         /*verify=*/ true,
                         /*verify_checksum*/ false,
                         error_msg,
                         &api_checker->api_classpath_)) {
      return nullptr;
    }
  }
  return api_checker.release();
}

bool ApiChecker::ShouldDenyAccess(ArtMethod* art_method) const {
  bool found = false;
  bool debug = false;  // TODO: remove
  for (const std::unique_ptr<const DexFile>& dex_file : api_classpath_) {
    const dex::TypeId* declaring_type_id =
        dex_file->FindTypeId(art_method->GetDeclaringClassDescriptor());
    if (declaring_type_id == nullptr) {
      if (debug) {
        LOG(WARNING) << "class " << art_method->GetDeclaringClassDescriptor();
      }
      continue;
    }
    const dex::StringId* name_id = dex_file->FindStringId(art_method->GetName());
    if (name_id == nullptr) {
      if (debug) {
        LOG(WARNING) << "name " << art_method->GetName();
      }
      continue;
    }

    dex::TypeIndex return_type_idx;
    std::vector<dex::TypeIndex> param_type_idxs;
    if (!dex_file->CreateTypeList(
            art_method->GetSignature().ToString().c_str(),
            &return_type_idx,
            &param_type_idxs)) {
      if (debug) {
        LOG(WARNING) << "type ";
      }
      continue;
    }
    const dex::ProtoId* proto_id = dex_file->FindProtoId(return_type_idx, param_type_idxs);
    if (proto_id == nullptr) {
      if (debug) {
        LOG(WARNING) << "proto ";
      }
      continue;
    }

    const dex::MethodId* method_id =
        dex_file->FindMethodId(*declaring_type_id, *name_id, *proto_id);
    if (method_id != nullptr) {
      found = true;
      // LOG(WARNING) << "found method " << art_method->PrettyMethod(true);
      break;
    } else if (debug) {
        LOG(WARNING) << "method ";
    }
  }

  if (!found) {
    LOG(WARNING) << "Deny for " << art_method->PrettyMethod(true);
  } else if (debug) {
    LOG(WARNING) << "Allow for " << art_method->PrettyMethod(true);
  }
  // Deny access if we didn't find the descriptor in the public api dex files.
  return !found;
}

bool ApiChecker::ShouldDenyAccess(ArtField* art_field) const {
  bool found = false;
  bool debug = false;
  for (const std::unique_ptr<const DexFile>& dex_file : api_classpath_) {
    std::string declaring_class;

    const dex::TypeId* declaring_type_id = dex_file->FindTypeId(
        art_field->GetDeclaringClass()->GetDescriptor(&declaring_class));
    if (declaring_type_id == nullptr) {
      if (debug) {
        LOG(WARNING) << "NO class: '" << declaring_class << "' -> " << art_field->GetDeclaringClass()->PrettyClass();
      }
      continue;
    }
    const dex::StringId* name_id = dex_file->FindStringId(art_field->GetName());
    if (name_id == nullptr) {
      if (debug) {
        LOG(WARNING) << "NO name";
      }
      continue;
    }
    const dex::TypeId* type_id = dex_file->FindTypeId(art_field->GetTypeDescriptor());
    if (type_id == nullptr) {
      if (debug) {
        LOG(WARNING) << "NO type";
      }
      continue;
    }

    const dex::FieldId* field_id = dex_file->FindFieldId(*declaring_type_id, *name_id, *type_id);
    if (field_id != nullptr) {
      found = true;
      break;
    } else {
      if (debug) {
        LOG(WARNING) << "NO field";
      }
    }
  }

  if (!found) {
    LOG(WARNING) << "Deny for " << ArtField::PrettyField(art_field, true);
  }

  // Deny access if we didn't find the descriptor in the public api dex files.
  return !found;
}

bool ApiChecker::ShouldDenyAccess(const char* descriptor) const {
  bool found = false;
  for (const std::unique_ptr<const DexFile>& dex_file : api_classpath_) {
    const dex::TypeId* type_id = dex_file->FindTypeId(descriptor);
    if (type_id != nullptr) {
      dex::TypeIndex type_idx = dex_file->GetIndexForTypeId(*type_id);
      if (dex_file->FindClassDef(type_idx) != nullptr) {
        found = true;
        break;
      }
    }
  }

  if (!found) {
    LOG(WARNING) << "Deny for " << descriptor;
  }

  // Deny access if we didn't find the descriptor in the public api dex files.
  return !found;
}

}  // namespace art
