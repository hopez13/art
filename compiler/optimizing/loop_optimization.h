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

#ifndef ART_COMPILER_OPTIMIZING_LOOP_OPTIMIZATION_H_
#define ART_COMPILER_OPTIMIZING_LOOP_OPTIMIZATION_H_

#include "induction_var_range.h"
#include "nodes.h"
#include "optimization.h"

namespace art {

class CompilerDriver;

/**
 * Loop optimizations. Builds a loop hierarchy and applies optimizations to
 * the detected nested loops, such as removal of dead induction and empty loops
 * and inner loop vectorization.
 */
class HLoopOptimization : public HOptimization {
 public:
  HLoopOptimization(HGraph* graph,
                    CompilerDriver* compiler_driver,
                    HInductionVarAnalysis* induction_analysis);

  void Run() OVERRIDE;

  static constexpr const char* kLoopOptimizationPassName = "loop_optimization";

 private:
  /**
   * A single loop inside the loop hierarchy representation.
   */
  struct LoopNode : public ArenaObject<kArenaAllocLoopOptimization> {
    explicit LoopNode(HLoopInformation* lp_info)
        : loop_info(lp_info),
          outer(nullptr),
          inner(nullptr),
          previous(nullptr),
          next(nullptr) {}
    HLoopInformation* loop_info;
    LoopNode* outer;
    LoopNode* inner;
    LoopNode* previous;
    LoopNode* next;
  };

  /*
   * Vectorization restrictions (bit mask).
   */
  enum VectorRestrictions {
    kNone     = 0,   // no restrictions
    kNoMul    = 1,   // no multiplication
    kNoDiv    = 2,   // no division
    kNoShift  = 4,   // no shift
    kNoShr    = 8,   // no arithmetic shift right
    kNoHiBits = 16,  // "wider" operations cannot bring in higher order bits
  };

  typedef std::tuple<HInstruction*,    // base
                     HInstruction*,    // offset + i
                     Primitive::Type,  // component type
                     bool> Reference;  // lhs/rhs


  // Loop setup and traversal.
  void LocalRun();
  void AddLoop(HLoopInformation* loop_info);
  void RemoveLoop(LoopNode* node);
  void TraverseLoopsInnerToOuter(LoopNode* node);

  // Optimization.
  void SimplifyInduction(LoopNode* node);
  void SimplifyBlocks(LoopNode* node);
  void OptimizeInnerLoop(LoopNode* node);

  // Vectorization analysis and synthesis.
  bool CanVectorize(LoopNode* node, HBasicBlock* block, int64_t tc);
  void Vectorize(LoopNode* node, HBasicBlock* block, HBasicBlock* exit, int64_t tc);
  void GenNewLoop(LoopNode* node,
                  HBasicBlock* block,
                  HBasicBlock* new_preheader,
                  HInstruction* lo,
                  HInstruction* hi,
                  HInstruction* step);
  bool VectorizeDef(LoopNode* node, HInstruction* instruction, bool gen);
  bool VectorizeUse(LoopNode* node,
                    HInstruction* instruction,
                    bool gen,
                    Primitive::Type type,
                    uint64_t restrictions);
  bool AcceptVectorType(Primitive::Type type, /*out*/ uint64_t* restrictions);
  bool AcceptVectorLength(uint32_t length);
  void GenVecInv(HInstruction* org, Primitive::Type type);
  void GenVecSub(HInstruction* org, HInstruction* off);
  void GenVecMem(HInstruction* org, HInstruction* opa, HInstruction* opb, Primitive::Type type);
  void GenVecOp(HInstruction* org, HInstruction* opa, HInstruction* opb, Primitive::Type type);

  // Helpers.
  bool IsPhiInduction(HPhi* phi, bool restrict_uses);
  bool IsSimpleLoopHeader(HBasicBlock* block);
  bool IsEmptyBody(HBasicBlock* block);
  bool IsOnlyUsedAfterLoop(HLoopInformation* loop_info,
                           HInstruction* instruction,
                           bool collect_loop_uses,
                           /*out*/ int32_t* use_count);
  bool IsUsedOutsideLoop(HLoopInformation* loop_info,
                         HInstruction* instruction);
  bool TryReplaceWithLastValue(HInstruction* instruction, HBasicBlock* block);
  bool TryAssignLastValue(HLoopInformation* loop_info,
                          HInstruction* instruction,
                          HBasicBlock* block,
                          bool collect_loop_uses);
  void RemoveDeadInstructions(const HInstructionList& list);

  // Compiler driver (to query ISA features).
  const CompilerDriver* compiler_driver_;

  // Range information based on prior induction variable analysis.
  InductionVarRange induction_range_;

  // Phase-local heap memory allocator for the loop optimizer. Storage obtained
  // through this allocator is immediately released when the loop optimizer is done.
  ArenaAllocator* loop_allocator_;

  // Global heap memory allocator. Used to build HIR.
  ArenaAllocator* global_allocator_;

  // Entries into the loop hierarchy representation. The hierarchy resides
  // in phase-local heap memory.
  LoopNode* top_loop_;
  LoopNode* last_loop_;

  // Temporary bookkeeping of a set of instructions.
  // Contents reside in phase-local heap memory.
  ArenaSet<HInstruction*>* iset_;

  // Counter that tracks how many induction cycles have been simplified. Useful
  // to trigger incremental updates of induction variable analysis of outer loops
  // when the induction of inner loops has changed.
  uint32_t induction_simplication_count_;

  // Flag that tracks if any simplifications have occurred.
  bool simplified_;

  // Vectorization bookkeeping (contents reside in phase-local heap memory).
  uint32_t vector_length_;  // number of "lanes" for selected packed type
  ArenaSet<Reference>* vector_refs_;  // array references in vector loop
  ArenaSafeMap<HInstruction*, HInstruction*>* vector_map_;  // code generation map
  HBasicBlock* vector_preheader_;  // preheader of the new loop
  HBasicBlock* vector_header_;  // header of the new loop
  HBasicBlock* vector_body_;  // body of the new loop
  HInstruction* vector_runtime_test_a_;
  HInstruction* vector_runtime_test_b_;  // defines a != b runtime test
  HPhi* vector_induc_;  // the Phi representing the normalized loop index
  bool simd_;  // selects SIMD (vector loop) or sequential (peeling/cleanup loop)

  friend class LoopOptimizationTest;

  DISALLOW_COPY_AND_ASSIGN(HLoopOptimization);
};

}  // namespace art

#endif  // ART_COMPILER_OPTIMIZING_LOOP_OPTIMIZATION_H_
