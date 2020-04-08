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

#include "nodes.h"
#include "optimization.h"

namespace art {

class DeconditionDeoptimize : public HOptimization {
 public:
  DeconditionDeoptimize(HGraph* graph,
                        const char* pass_name = kDeconditionDeoptimizePassName)
      : HOptimization(graph, pass_name) {}

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
  //           |            +----+-----+
  //           +----------->+Exit Block|
  //                        +----------+
  bool Run() override;

  static constexpr const char* kDeconditionDeoptimizePassName = "DeconditionDeoptimize";

 private:
  void VisitBasicBlock(HBasicBlock* block);

  DISALLOW_COPY_AND_ASSIGN(DeconditionDeoptimize);
};

}  // namespace art

#endif  // ART_COMPILER_OPTIMIZING_DECONDITION_DEOPTIMIZE_H_
