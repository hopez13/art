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

#include "decondition_deoptimize.h"

#include "android-base/stringprintf.h"
#include "base/arena_bit_vector.h"
#include "base/bit_vector-inl.h"
#include "base/scoped_arena_allocator.h"
#include "base/scoped_arena_containers.h"
#include "base/utils.h"
#include "base/value_object.h"
#include "nodes.h"
#include "optimizing/optimizing_compiler_stats.h"
#include "side_effects_analysis.h"

using android::base::StringPrintf;
namespace art {

class DeoptimizationRemover : public ValueObject {
 public:
  DeoptimizationRemover(HGraph* graph, ArenaAllocKind kind, OptimizingCompilerStats* stats)
      : graph_(graph), required_deopts_(graph_->GetAllocator()->Adapter(kind)), stats_(stats) {}

  ~DeoptimizationRemover() {
    DCHECK(required_deopts_.empty());
  }

  void Finalize();
  void AddPredicatedDeoptimization(HDeoptimizeMarker* deopt);

  HGraph* GetGraph() const {
    return graph_;
  }

 private:
  HGraph* graph_;

  ArenaVector<HDeoptimizeMarker*> required_deopts_;
  OptimizingCompilerStats* stats_;
};

void DeoptimizationRemover::Finalize() {
  if (required_deopts_.empty()) {
    return;
  }
  HGraph* graph = GetGraph();
  for (HDeoptimizeMarker* pd : required_deopts_) {
    HBasicBlock* new_pred = pd->GetBlock();
    HBasicBlock* new_block = new_pred->SplitBefore(pd);
    DCHECK(new_pred->GetLastInstruction()->IsGoto());
    HInstruction* cond = pd->GetCondition();
    HInstruction* goto_inst = new_pred->GetLastInstruction();
    HInstruction* if_inst = new (graph->GetAllocator()) HIf(cond);

    new_pred->ReplaceAndRemoveInstructionWith(goto_inst, if_inst);

    // Make a deopt block.
    HBasicBlock* deopt_block = new (graph->GetAllocator()) HBasicBlock(graph, pd->GetDexPc());
    graph->AddBlock(deopt_block);
    HDeoptimize* new_deopt =
        new (graph->GetAllocator()) HDeoptimize(pd->GetDeoptimizationKind(), pd->GetDexPc());
    deopt_block->AddInstruction(new_deopt);
    new_deopt->CopyEnvironmentFrom(pd->GetEnvironment());
    deopt_block->AddSuccessor(graph->GetExitBlock());
    new_pred->AddSuccessor(deopt_block);
    // The true branch is the deopt.
    new_pred->SwapSuccessors();
    new_block->RemoveInstruction(pd);
    MaybeRecordStat(stats_, MethodCompilationStat::kDeoptimizeBlockAdded);
  }
  GetGraph()->ClearLoopInformation();
  GetGraph()->ClearDominanceInformation();
  GetGraph()->BuildDominatorTree();
  required_deopts_.clear();
}

void DeoptimizationRemover::AddPredicatedDeoptimization(HDeoptimizeMarker* deopt) {
  required_deopts_.push_back(deopt);
}

class GuardRemover : public ValueObject {
 public:
  explicit GuardRemover(HGraph* graph, OptimizingCompilerStats* stats)
      : graph_(graph), deopt_remover_(graph_, ArenaAllocKind::kArenaAllocMisc, stats) {}

  void Run() {
    for (HBasicBlock* bb : graph_->GetReversePostOrder()) {
      VisitBasicBlock(bb);
    }
    deopt_remover_.Finalize();
  }

 private:
  void VisitBasicBlock(HBasicBlock* bb) {
    HInstruction* current_instruction = bb->GetFirstInstruction();
    DCHECK(current_instruction != nullptr)
        << "Block without instructions found! " << bb->GetBlockId();
    HInstruction* next;
    do {
      next = current_instruction->GetNext();
      if (current_instruction->IsDeoptimizeGuard()) {
        HDeoptimizeGuard* guard = current_instruction->AsDeoptimizeGuard();
        HInstruction* guarded = guard->GuardedInput();
        HInstruction* cond = guard->Condition();
        // restore the original value in users.
        // Make the deopt we will actually use.
        HDeoptimizeMarker* deopt = new (graph_->GetAllocator())
            HDeoptimizeMarker(cond, guard->GetDeoptimizationKind(), guard->GetDexPc());
        bb->InsertInstructionBefore(deopt, guard);
        deopt->CopyEnvironmentFrom(guard->GetEnvironment());
        guard->ReplaceWith(guarded);
        bb->RemoveInstruction(guard);
        deopt_remover_.AddPredicatedDeoptimization(deopt);
        LOG(INFO) << graph_->GetMethodName() << ": Replacing guard deopt " << guard->GetId() << " with deopt " << deopt->GetId();
      } else if (current_instruction->IsDeoptimizeMarker()) {
        deopt_remover_.AddPredicatedDeoptimization(current_instruction->AsDeoptimizeMarker());
      } else {
        CHECK(current_instruction->Emitable()) << current_instruction->DebugName() << " id: " << current_instruction->GetId();
      }
    } while ((current_instruction = next) != nullptr);
  }

  HGraph* graph_;
  DeoptimizationRemover deopt_remover_;
};

bool DeconditionDeoptimize::Run() {
  GuardRemover dr(graph_, stats_);
  dr.Run();

  for (HBasicBlock* bb : graph_->GetReversePostOrder()) {
    HInstructionIterator hii(bb->GetInstructions());
    for (HInstruction* hi = hii.Current(); !hii.Done(); hii.Advance(), hi = hii.Current()) {
      CHECK(hi->Emitable()) << hi->DebugName() << " id: " << hi->GetId();
    }
  }
  return true;
}

}  // namespace art
