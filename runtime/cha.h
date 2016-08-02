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

#ifndef ART_RUNTIME_CHA_H_
#define ART_RUNTIME_CHA_H_

#include "art_method.h"
#include "base/enums.h"
#include "base/mutex.h"
#include "handle.h"
#include "mirror/class.h"
#include "oat_quick_method_header.h"
#include <unordered_map>
#include <unordered_set>

namespace art {

class ClassHierarchyAnalysis {
 public:
  ClassHierarchyAnalysis() {}

  // Returns compiled code that assumes that `method` has single-implementation.
  std::vector<std::pair<ArtMethod*, OatQuickMethodHeader*>>* GetDependents(ArtMethod* method)
      REQUIRES(Locks::cha_lock_);

  // Add a dependency that compiled code with `dependent_header` for `dependent_method`
  // assumes that virtual `method` has single-implementation.
  void AddDependency(ArtMethod* method,
                     ArtMethod* dependent_method,
                     OatQuickMethodHeader* dependent_header) REQUIRES(Locks::cha_lock_);

  // Remove dependency tracking for compiled code that assumes that
  // `method` has single-implementation.
  void RemoveDependencyFor(ArtMethod* method) REQUIRES(Locks::cha_lock_);

  // Update CHA info for methods that `klass` overrides.
  void Update(Handle<mirror::Class> klass) REQUIRES_SHARED(Locks::mutator_lock_);

 private:
  void InitSingleImplementationFlag(Handle<mirror::Class> klass, ArtMethod* method)
      REQUIRES_SHARED(Locks::mutator_lock_);

  // `virtual_method` in `klass` overrides `method_in_super`.
  // This will invalidate some assumptions on single-implementation.
  // Append methods that should have their single-implementation flag invalidated
  // to `invalidated_single_impl_methods`.
  void CheckSingleImplementationInfo(
      Handle<mirror::Class> klass,
      ArtMethod* virtual_method,
      ArtMethod* method_in_super,
      std::unordered_set<ArtMethod*>& invalidated_single_impl_methods)
      REQUIRES_SHARED(Locks::mutator_lock_);

  // A map that maps a method to a set of compiled code that assumes that method has a
  // single implementation, which is used to do CHA-based devirtualization.
  std::unordered_map<ArtMethod*,
      std::vector<std::pair<ArtMethod*, OatQuickMethodHeader*>>*> cha_dependency_map_
          GUARDED_BY(Locks::cha_lock_);

  DISALLOW_COPY_AND_ASSIGN(ClassHierarchyAnalysis);
};

}  // namespace art

#endif  // ART_RUNTIME_CHA_H_
