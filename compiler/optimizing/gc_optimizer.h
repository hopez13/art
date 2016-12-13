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

#ifndef ART_COMPILER_OPTIMIZING_GC_OPTIMIZER_H_
#define ART_COMPILER_OPTIMIZING_GC_OPTIMIZER_H_

#include "nodes.h"
#include "optimization.h"

namespace art {

/**
 * Optimization pass performing optimizations garbage collection
 * related optimizations.
 */
class GcOptimizer : public HOptimization {
 public:
  GcOptimizer(HGraph* graph, const char* name = kGcOptimizerPassName)
      : HOptimization(graph, name) {}

  void Run() OVERRIDE;

  static constexpr const char* kGcOptimizerPassName = "gc_optimizer";

 private:
  DISALLOW_COPY_AND_ASSIGN(GcOptimizer);
};

}  // namespace art

#endif  // ART_COMPILER_OPTIMIZING_GC_OPTIMIZER_H_
