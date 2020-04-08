/*
 * Copyright (C) 2020 The Android Open Source Project
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

#ifndef ART_COMPILER_OPTIMIZING_DECONDITION_DEOPTIMIZE_H_
#define ART_COMPILER_OPTIMIZING_DECONDITION_DEOPTIMIZE_H_

#include <type_traits>
#include "base/value_object.h"
#include "nodes.h"
#include "optimization.h"
#include "optimizing/optimizing_compiler_stats.h"

namespace art {

// Transform the graph to move all deoptimize nodes to being in their own
// block with a constant-true condition.
//
// for example Transform
//                 +-------+
//                 |Block 1|
//                 +---+---+
//                     |
//                     v
//             +-------+-------+
//             |FOO...         |
//             |C1 <- Condition|
//             |Deoptimize [C1]|
//             |BAR...         |
//             +-------+-------+
//                     |
//                     v
//                   +-+-+
//                   |...|
//                   +-+-+
//                     |
//                     v
//                +----+-----+
//                |Exit Block|
//                +----------+
//
// Into
//                        +-------+
//                        |Block 1|
//                        +---+---+
//                            |
//                            v
//                    +-------+-------+
//                    |FOO...         |
//                    |C1 <- Condition|
//                    |If [C1]        |
//                    +-+------+------+
//                      |True  |False
//                      |      v
//           +----------+   +--+---+
//           v              |BAR...|
//  +--------+--------+     +--+---+
//  |Deoptimize [True]|        |
//  +--------+--------+        v
//           |               +-+-+
//           |               |...|
//           |               +-+-+
//           |                 |
//           |                 v
//           |            +----+-----+
//           +----------->+Exit Block|
//                        +----------+
class DeconditionDeoptimize : public HOptimization {
 public:
  DeconditionDeoptimize(HGraph* graph,
                        OptimizingCompilerStats* stats = nullptr,
                        const char* pass_name = kDeconditionDeoptimizePassName)
      : HOptimization(graph, pass_name), stats_(stats) {}

  // Transform the graph to move all DeoptimizeGaurd nodes to being in their own
  // block and use the guarded value directly in other instructions.
  bool Run() override;

  static constexpr const char* kDeconditionDeoptimizePassName = "DeconditionDeoptimize";

 private:
  OptimizingCompilerStats* stats_;

  DISALLOW_COPY_AND_ASSIGN(DeconditionDeoptimize);
};



}  // namespace art

#endif  // ART_COMPILER_OPTIMIZING_DECONDITION_DEOPTIMIZE_H_
