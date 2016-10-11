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

#ifndef ART_COMPILER_OPTIMIZING_SCHEDULER_ARM_H_
#define ART_COMPILER_OPTIMIZING_SCHEDULER_ARM_H_

#include "scheduler.h"

namespace art {
namespace arm {

static constexpr int32_t kArmMemoryLoadLatency = 5;
static constexpr int32_t kArmMemoryStoreLatency = 3;

static constexpr int32_t kArmCallInternalLatency = 10;
static constexpr int32_t kArmCallLatency = 5;

// ARM instruction latency.
// We currently assume that all arm CPUs share the same instruction latency list.
static constexpr int32_t kArmIntegerOpLatency = 2;
static constexpr int32_t kArmFloatingPointOpLatency = 5;


static constexpr int32_t kArmDivDoubleLatency = 30;
static constexpr int32_t kArmDivFloatingPointLatency = 15;
static constexpr int32_t kArmDivIntegerLatency = 5;
static constexpr int32_t kArmLoadStringInternalLatency = 7;
static constexpr int32_t kArmMulFloatingPointLatency = 6;
static constexpr int32_t kArmMulIntegerLatency = 6;
static constexpr int32_t kArmTypeConversionFloatingPointIntegerLatency = 5;

class SchedulingLatencyVisitorARM : public SchedulingLatencyVisitor {
 public:
  // Default visitor for instructions not handled specifically below.
  void VisitInstruction(HInstruction* ATTRIBUTE_UNUSED) {
    last_visited_latency_ = kArmIntegerOpLatency;
  }

// We add a second unused parameter to be able to use this macro like the others
// defined in `nodes.h`.
#define FOR_EACH_SCHEDULED_COMMON_INSTRUCTION(M) \
  M(ArrayGet         , unused)                   \
  M(ArrayLength      , unused)                   \
  M(ArraySet         , unused)                   \
  M(BinaryOperation  , unused)                   \
  M(BoundsCheck      , unused)                   \
  M(Div              , unused)                   \
  M(InstanceFieldGet , unused)                   \
  M(InstanceOf       , unused)                   \
  M(Invoke           , unused)                   \
  M(LoadString       , unused)                   \
  M(Mul              , unused)                   \
  M(NewArray         , unused)                   \
  M(NewInstance      , unused)                   \
  M(Rem              , unused)                   \
  M(StaticFieldGet   , unused)                   \
  M(SuspendCheck     , unused)                   \
  M(TypeConversion   , unused)

#define FOR_EACH_SCHEDULED_SHARED_INSTRUCTION(M) \
  M(BitwiseNegatedRight, unused)                 \
  M(MultiplyAccumulate, unused)                  \
  M(IntermediateAddress, unused)

#define DECLARE_VISIT_INSTRUCTION(type, unused)  \
  void Visit##type(H##type* instruction) OVERRIDE;

  FOR_EACH_SCHEDULED_COMMON_INSTRUCTION(DECLARE_VISIT_INSTRUCTION)
  FOR_EACH_SCHEDULED_SHARED_INSTRUCTION(DECLARE_VISIT_INSTRUCTION)
  FOR_EACH_CONCRETE_INSTRUCTION_ARM(DECLARE_VISIT_INSTRUCTION)

#undef DECLARE_VISIT_INSTRUCTION
};

class HSchedulerARM : public HScheduler {
 public:
  HSchedulerARM(ArenaAllocator* arena, SchedulingNodeSelector* selector)
      : HScheduler(arena, &arm_latency_visitor_, selector) {}
  ~HSchedulerARM() OVERRIDE {}

  bool IsSchedulable(const HInstruction* instruction) const OVERRIDE {
#define IS_INSTRUCTION(type, unused) instruction->Is##type() ||
    if (FOR_EACH_SCHEDULED_SHARED_INSTRUCTION(IS_INSTRUCTION)
        FOR_EACH_CONCRETE_INSTRUCTION_ARM(IS_INSTRUCTION) false) {
      return true;
    }
#undef IS_INSTRUCTION
    return HScheduler::IsSchedulable(instruction);
  }

 private:
  SchedulingLatencyVisitorARM arm_latency_visitor_;
  DISALLOW_COPY_AND_ASSIGN(HSchedulerARM);
};

}  // namespace arm
}  // namespace art

#endif  // ART_COMPILER_OPTIMIZING_SCHEDULER_ARM_H_
