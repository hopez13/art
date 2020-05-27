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
#include "base/arena_allocator.h"
#include "base/scoped_arena_containers.h"
#include "base/value_object.h"
#include "nodes.h"
#include "optimization.h"

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

class DeoptimizationRemover : public ValueObject {
 public:
  DeoptimizationRemover(HGraph* graph, ScopedArenaAllocator& alloc, ArenaAllocKind kind)
      : graph_(graph), required_deopts_(alloc.Adapter(kind)) {}

  ~DeoptimizationRemover() {
    DCHECK(required_deopts_.empty());
  }

  void AdjustControlFlow();
  void AddPredicatedDeoptimization(HDeoptimize* deopt);

  HGraph* GetGraph() const {
    return graph_;
  }

 private:
  HGraph* graph_;

  ScopedArenaVector<HDeoptimize*> required_deopts_;
};

}  // namespace art

#endif  // ART_COMPILER_OPTIMIZING_DECONDITION_DEOPTIMIZE_H_
