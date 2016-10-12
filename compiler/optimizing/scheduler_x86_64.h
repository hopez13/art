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

#ifndef ART_COMPILER_OPTIMIZING_SCHEDULER_X86_64_H_
#define ART_COMPILER_OPTIMIZING_SCHEDULER_X86_64_H_

#include "scheduler.h"

namespace art {
namespace x86_64 {

// TODO: Check the Intel optimization manuals to find good latencies.
static constexpr int32_t kX86_64MemoryLoadLatency = 5;
static constexpr int32_t kX86_64MemoryStoreLatency = 3;

static constexpr int32_t kX86_64CallInternalLatency = 10;
static constexpr int32_t kX86_64CallLatency = 5;

// X86_64 instruction latency.
// We currently assume that all x86_64 CPUs share the same instruction latency list.
static constexpr int32_t kX86_64IntegerOpLatency = 2;
static constexpr int32_t kX86_64FloatingPointOpLatency = 5;


static constexpr int32_t kX86_64DivDoubleLatency = 30;
static constexpr int32_t kX86_64DivFloatingPointLatency = 15;
static constexpr int32_t kX86_64DivIntegerLatency = 30;
static constexpr int32_t kX86_64DivLongLatency = 50;
static constexpr int32_t kX86_64LoadStringInternalLatency = 7;
static constexpr int32_t kX86_64MulFloatingPointLatency = 6;
static constexpr int32_t kX86_64MulIntegerLatency = 6;
static constexpr int32_t kX86_64TypeConversionFloatingPointIntegerLatency = 5;

class SchedulingLatencyVisitorX86_64 : public SchedulingLatencyVisitor {
 public:
  // Default visitor for instructions not handled specifically below.
  void VisitInstruction(HInstruction* ATTRIBUTE_UNUSED) {
    last_visited_latency_ = kX86_64IntegerOpLatency;
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

#define DECLARE_VISIT_INSTRUCTION(type, unused)  \
  void Visit##type(H##type* instruction) OVERRIDE;

  FOR_EACH_SCHEDULED_COMMON_INSTRUCTION(DECLARE_VISIT_INSTRUCTION)
  FOR_EACH_CONCRETE_INSTRUCTION_X86_64(DECLARE_VISIT_INSTRUCTION)

#undef DECLARE_VISIT_INSTRUCTION
};

class HSchedulerX86_64 : public HScheduler {
 public:
  HSchedulerX86_64(ArenaAllocator* arena, SchedulingNodeSelector* selector)
      : HScheduler(arena, &x86_64_latency_visitor_, selector) {}
  ~HSchedulerX86_64() OVERRIDE {}

  bool IsSchedulable(const HInstruction* instruction) const OVERRIDE {
#define IS_INSTRUCTION(type, unused) instruction->Is##type() ||
    if (FOR_EACH_CONCRETE_INSTRUCTION_X86_64(IS_INSTRUCTION) false) {
      return true;
    }
#undef IS_INSTRUCTION
    return HScheduler::IsSchedulable(instruction);
  }

 private:
  SchedulingLatencyVisitorX86_64 x86_64_latency_visitor_;
  DISALLOW_COPY_AND_ASSIGN(HSchedulerX86_64);
};

}  // namespace x86_64
}  // namespace art

#endif  // ART_COMPILER_OPTIMIZING_SCHEDULER_X86_64_H_
