/*
 * Copyright (C) 2018 The Android Open Source Project
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

#include "loop_analysis.h"

#include "base/bit_vector-inl.h"
#include "induction_var_range.h"

namespace art {

void LoopAnalysis::CalculateLoopBasicProperties(HLoopInformation* loop_info,
                                                LoopAnalysisInfo* analysis_results,
                                                int64_t trip_count) {
  analysis_results->trip_count_ = trip_count;

  for (HBlocksInLoopIterator block_it(*loop_info);
       !block_it.Done();
       block_it.Advance()) {
    HBasicBlock* block = block_it.Current();

    // Check whether one of the successor is loop exit.
    for (HBasicBlock* successor : block->GetSuccessors()) {
      if (!loop_info->Contains(*successor)) {
        analysis_results->exits_num_++;

        // We track number of invariant loop exits which correspond to HIf instruction and
        // can be eliminated by loop peeling; other control flow instruction are ignored and will
        // not cause loop peeling to happen as they either cannot be inside a loop, or by
        // definition cannot be loop exits (unconditional instructions), or are not beneficial for
        // the optimization.
        HIf* hif = block->GetLastInstruction()->AsIf();
        if (hif != nullptr && !loop_info->Contains(*hif->InputAt(0)->GetBlock())) {
          analysis_results->invariant_exits_num_++;
        }
      }
    }

    for (HInstructionIterator it(block->GetInstructions()); !it.Done(); it.Advance()) {
      HInstruction* instruction = it.Current();
      if (it.Current()->GetType() == DataType::Type::kInt64) {
        analysis_results->has_long_type_instructions_ = true;
      }
      if (MakesScalarPeelingUnrollingNonBeneficial(instruction)) {
        analysis_results->has_instructions_preventing_scalar_peeling_ = true;
        analysis_results->has_instructions_preventing_scalar_unrolling_ = true;
      }
      analysis_results->instr_num_++;
    }
    analysis_results->bb_num_++;
  }
}

int64_t LoopAnalysis::GetLoopTripCount(HLoopInformation* loop_info,
                                       const InductionVarRange* induction_range) {
  int64_t trip_count;
  if (!induction_range->HasKnownTripCount(loop_info, &trip_count)) {
    trip_count = LoopAnalysisInfo::kUnknownTripCount;
  }
  return trip_count;
}

// Default implementation of loop helper; used for all targets unless a custom implementation
// is provided. Enables scalar loop peeling and unrolling with the most conservative heuristics.
class ArchDefaultLoopHelper : public ArchNoOptsLoopHelper {
 public:
  // Scalar loop unrolling parameters and heuristics.
  //
  // Maximum possible unrolling factor.
  static constexpr uint32_t kScalarMaxUnrollFactor = 2;
  // Loop's maximum instruction count. Loops with higher count will not be peeled/unrolled.
  static constexpr uint32_t kScalarHeuristicMaxBodySizeInstr = 17;
  // Loop's maximum basic block count. Loops with higher count will not be peeled/unrolled.
  static constexpr uint32_t kScalarHeuristicMaxBodySizeBlocks = 6;
  // Maximum number of instructions to be created as a result of full unrolling.
  static constexpr uint32_t kScalarHeuristicFullyUnrolledMaxInstrThreshold = 35;

  bool IsLoopNonBeneficialForScalarOpts(LoopAnalysisInfo* analysis_info) const override {
    return analysis_info->HasLongTypeInstructions() ||
           IsLoopTooBig(analysis_info,
                        kScalarHeuristicMaxBodySizeInstr,
                        kScalarHeuristicMaxBodySizeBlocks);
  }

  uint32_t GetScalarUnrollingFactor(const LoopAnalysisInfo* analysis_info) const override {
    int64_t trip_count = analysis_info->GetTripCount();
    // Unroll only loops with known trip count.
    if (trip_count == LoopAnalysisInfo::kUnknownTripCount) {
      return LoopAnalysisInfo::kNoUnrollingFactor;
    }
    uint32_t desired_unrolling_factor = kScalarMaxUnrollFactor;
    if (trip_count < desired_unrolling_factor || trip_count % desired_unrolling_factor != 0) {
      return LoopAnalysisInfo::kNoUnrollingFactor;
    }

    return desired_unrolling_factor;
  }

  bool IsLoopPeelingEnabled() const override { return true; }

  bool IsFullUnrollingBeneficial(LoopAnalysisInfo* analysis_info) const override {
    int64_t trip_count = analysis_info->GetTripCount();
    // We assume that trip count is known.
    DCHECK_NE(trip_count, LoopAnalysisInfo::kUnknownTripCount);
    size_t instr_num = analysis_info->GetNumberOfInstructions();
    return (trip_count * instr_num < kScalarHeuristicFullyUnrolledMaxInstrThreshold);
  }

 protected:
  bool IsLoopTooBig(LoopAnalysisInfo* loop_analysis_info,
                    size_t instr_threshold,
                    size_t bb_threshold) const {
    size_t instr_num = loop_analysis_info->GetNumberOfInstructions();
    size_t bb_num = loop_analysis_info->GetNumberOfBasicBlocks();
    return (instr_num >= instr_threshold || bb_num >= bb_threshold);
  }
};

// Custom implementation of loop helper for arm64 target. Enables heuristics for scalar loop
// peeling and unrolling and supports SIMD loop unrolling.
class Arm64LoopHelper : public ArchDefaultLoopHelper {
 public:
  // SIMD loop unrolling parameters and heuristics.
  //
  // Maximum possible unrolling factor.
  static constexpr uint32_t kArm64SimdMaxUnrollFactor = 8;
  // Loop's maximum instruction count. Loops with higher count will not be unrolled.
  static constexpr uint32_t kArm64SimdHeuristicMaxBodySizeInstr = 50;

  // Loop's maximum instruction count. Loops with higher count will not be peeled/unrolled.
  static constexpr uint32_t kArm64ScalarHeuristicMaxBodySizeInstr = 40;
  // Loop's maximum basic block count. Loops with higher count will not be peeled/unrolled.
  static constexpr uint32_t kArm64ScalarHeuristicMaxBodySizeBlocks = 8;

  bool IsLoopNonBeneficialForScalarOpts(LoopAnalysisInfo* loop_analysis_info) const override {
    return IsLoopTooBig(loop_analysis_info,
                        kArm64ScalarHeuristicMaxBodySizeInstr,
                        kArm64ScalarHeuristicMaxBodySizeBlocks);
  }

  uint32_t GetSIMDUnrollingFactor(HBasicBlock* block,
                                  int64_t trip_count,
                                  uint32_t max_peel,
                                  uint32_t vector_length) const override {
    // Don't unroll with insufficient iterations.
    // TODO: Unroll loops with unknown trip count.
    DCHECK_NE(vector_length, 0u);
    if (trip_count < (2 * vector_length + max_peel)) {
      return LoopAnalysisInfo::kNoUnrollingFactor;
    }
    // Don't unroll for large loop body size.
    uint32_t instruction_count = block->GetInstructions().CountSize();
    if (instruction_count >= kArm64SimdHeuristicMaxBodySizeInstr) {
      return LoopAnalysisInfo::kNoUnrollingFactor;
    }
    // Find a beneficial unroll factor with the following restrictions:
    //  - At least one iteration of the transformed loop should be executed.
    //  - The loop body shouldn't be "too big" (heuristic).

    uint32_t uf1 = kArm64SimdHeuristicMaxBodySizeInstr / instruction_count;
    uint32_t uf2 = (trip_count - max_peel) / vector_length;
    uint32_t unroll_factor =
        TruncToPowerOfTwo(std::min({uf1, uf2, kArm64SimdMaxUnrollFactor}));
    DCHECK_GE(unroll_factor, 1u);
    return unroll_factor;
  }
};

// Custom implementation of loop helper for X86_64 target. Enables heuristics for scalar loop
// peeling and unrolling and supports SIMD loop unrolling.
class X86_64LoopHelper : public ArchDefaultLoopHelper {
  // map having instruction count for most used IRs
  // Few IRs generate different number of instructions based on input and result type.
  // We checked top java apps, benchmarks and used the most generated instruction count.
  std::map<std::string, uint32_t> map_IR_Inst = {
    {"Abs", 3},
    {"Add", 1},
    {"And", 1},
    {"ArrayLength", 1},
    {"ArrayGet", 1},
    {"ArraySet", 1},
    {"BoundsCheck", 2},
    {"CheckCast", 9},
    {"Div", 8},
    {"DivZeroCheck", 2},
    {"Equal", 3},
    {"GreaterThan", 3},
    {"GreaterThanOrEqual", 3},
    {"If", 2},
    {"InstanceFieldGet", 2},
    {"InstanceFieldSet", 1},
    {"LessThan", 3},
    {"LessThanOrEqual", 3},
    {"Max", 2},
    {"Min", 2},
    {"Mul", 1},
    {"NotEqual", 3},
    {"Or", 1},
    {"Rem", 11},
    {"Select", 2},
    {"Shl", 1},
    {"Shr", 1},
    {"Sub", 1},
    {"TypeConversion", 1},
    {"UShr", 1},
    {"VecReplicateScalar", 2},
    {"VecExtractScalar", 1},
    {"VecReduce", 4},
    {"VecNeg", 2},
    {"VecAbs", 4},
    {"VecNot", 3},
    {"VecAdd", 1},
    {"VecSub", 1},
    {"VecMul", 1},
    {"VecDiv", 1},
    {"VecMax", 1},
    {"VecMin", 1},
    {"VecOr", 1},
    {"VecXor", 1},
    {"VecShl", 1},
    {"VecShr", 1},
    {"VecLoad", 2},
    {"VecStore", 2},
    {"Xor", 1}};

  // Maximum possible unrolling factor.
  static constexpr uint32_t kX86_64MaxUnrollFactor = 2;  // pow(2,2) = 4

  // Maximum total instruction count after unrolling. Loops with higher count will not be unrolled.
  // this variable is set according to utilize LSD (loop stream decoder) on IA
  // For atom processor (silvermont & goldmont), LSD size is 28
  // TODO - identify architecture and LSD size at runtime
  static constexpr uint32_t kX86_64UnrolledMaxBodySizeInstr = 28;

  // Loop's maximum basic block count. Loops with higher count will not be partial unrolled (unknown iterations).
  static constexpr uint32_t kX86_64UnknownIterMaxBodySizeBlocks = 2;

  uint32_t GetUnrollingFactor(HLoopInformation* loop_info, HBasicBlock* header) const;

 public:
  uint32_t GetSIMDUnrollingFactor(HBasicBlock* block,
                                  int64_t trip_count,
                                  uint32_t max_peel,
                                  uint32_t vector_length) const override {
    DCHECK_NE(vector_length, 0u);
    HLoopInformation* loop_info = block->GetLoopInformation();
    DCHECK(loop_info);
    HBasicBlock* header = loop_info->GetHeader();
    DCHECK(header);
    uint32_t unroll_factor = 0;

    if ((trip_count == 0) || (trip_count == LoopAnalysisInfo::kUnknownTripCount)) {
      // Don't unroll for large loop body size.
      unroll_factor = GetUnrollingFactor(loop_info, header);
      if (unroll_factor <= 1) {
        return LoopAnalysisInfo::kNoUnrollingFactor;
      }
    } else {
      // Don't unroll with insufficient iterations.
      if (trip_count < (2 * vector_length + max_peel)) {
        return LoopAnalysisInfo::kNoUnrollingFactor;
      }

      // Don't unroll for large loop body size.
      uint32_t unroll_cnt = GetUnrollingFactor(loop_info, header);
      if (unroll_cnt <= 1) {
        return LoopAnalysisInfo::kNoUnrollingFactor;
      }

      // Find a beneficial unroll factor with the following restrictions:
      //  - At least one iteration of the transformed loop should be executed.
      //  - The loop body shouldn't be "too big" (heuristic).
      uint32_t uf2 = (trip_count - max_peel) / vector_length;
      unroll_factor = TruncToPowerOfTwo(std::min({uf2, unroll_cnt}));
      DCHECK_GE(unroll_factor, 1u);
    }

    return unroll_factor;
  }
};

uint32_t X86_64LoopHelper::GetUnrollingFactor(HLoopInformation* loop_info, HBasicBlock* header) const {
  uint32_t num_inst = 0, num_inst_header = 0, num_inst_oth = 0;
  for (HBlocksInLoopIterator it(*loop_info); !it.Done(); it.Advance()) {
    HBasicBlock* block = it.Current();
    DCHECK(block);
    num_inst = 0;

    for (HInstructionIterator it1(block->GetInstructions()); !it1.Done(); it1.Advance()) {
      HInstruction* inst = it1.Current();
      DCHECK(inst);

      // SuspendCheck inside loop is handled with Goto
      if (inst->IsPhi() || inst->IsSuspendCheck() || inst->IsGoto()) continue;

      std::string t_str = inst->DebugName();
      auto iter = map_IR_Inst.find(t_str);
      if (iter != map_IR_Inst.end()) {
        num_inst += iter->second;
      } else {
        num_inst++;
      }
    }

    if (block == header) {
      num_inst_header = num_inst;
    } else {
      num_inst_oth += num_inst;
    }
  }

  // calculate actual unroll factor
  // "-3" instrutions for Goto
  uint32_t unrolling_factor = kX86_64MaxUnrollFactor;
  uint32_t unrolling_inst = kX86_64UnrolledMaxBodySizeInstr;
  uint32_t desired_size = unrolling_inst - num_inst_header - 3;
  if (desired_size < (2*num_inst_oth)) {
    return 1;
  }

  while (unrolling_factor > 0) {
    if ((desired_size >> unrolling_factor) >= num_inst_oth) {
      break;
    }
    unrolling_factor--;
  }

  return (1 << unrolling_factor);
}

ArchNoOptsLoopHelper* ArchNoOptsLoopHelper::Create(InstructionSet isa,
                                                   ArenaAllocator* allocator) {
  switch (isa) {
    case InstructionSet::kArm64: {
      return new (allocator) Arm64LoopHelper;
    }
    case InstructionSet::kX86_64: {
      return new (allocator) X86_64LoopHelper;
    }
    default: {
      return new (allocator) ArchDefaultLoopHelper;
    }
  }
}

}  // namespace art
