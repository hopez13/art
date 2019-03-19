/*
 * Copyright (C) 2019 The Android Open Source Project
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

#include "iv_simplifier.h"
#include <tuple>

namespace art {


typedef std::tuple<HInstruction*, HConstant*, HConstant*> triple;
//
// public methods
//
IvStrengthReduction::IvStrengthReduction(HInductionVarAnalysis* induction_analysis)
    : induction_analysis_(induction_analysis),
      biv_increment_(nullptr) {
  DCHECK(induction_analysis != nullptr);
}

/*Helper Functions*/
static HConstant* GetNewConstant(HInstruction::InstructionKind kind, HConstant* cst1, HConstant* cst2) {
  DataType::Type type = cst1->GetType();
  HConstant* new_cst = nullptr;
  HGraph* graph = cst1->GetBlock()->GetGraph();
  if (DataType::IsIntegralType(type)) {
    int64_t new_val = 0;
    switch (kind) {
      case HInstruction::kAdd :
        new_val = Int64FromConstant(cst1) + Int64FromConstant(cst2);
        break;
      case HInstruction::kSub:
        new_val = Int64FromConstant(cst1) - Int64FromConstant(cst2);
        break;
      case HInstruction::kMul:
        new_val = Int64FromConstant(cst1) * Int64FromConstant(cst2);
        break;
      default :
      UNREACHABLE();
    }
     new_cst = graph->GetConstant(type, new_val);
     return new_cst;
    } else if (cst1->IsDoubleConstant()) {
      double new_val = 0.0;
      switch (kind) {
        case HInstruction::kAdd:
          new_val = cst1->AsDoubleConstant()->GetValue() + cst2->AsDoubleConstant()->GetValue();
          break;
        case HInstruction::kSub:
          new_val = cst1->AsDoubleConstant()->GetValue() - (cst2->AsDoubleConstant()->GetValue());
          break;
        case HInstruction::kMul:
          new_val = cst1->AsDoubleConstant()->GetValue() * cst2->AsDoubleConstant()->GetValue();
          break;
        default :
          UNREACHABLE();
      }
      new_cst = graph->GetDoubleConstant(new_val);
      return new_cst;
    } else if (cst1->IsFloatConstant()) {
      float new_val = 0.0;
      switch (kind) {
        case HInstruction::kAdd:
          new_val = cst1->AsFloatConstant()->GetValue() + cst2->AsFloatConstant()->GetValue();
          break;
        case HInstruction::kSub:
          new_val = cst1->AsFloatConstant()->GetValue() - cst2->AsFloatConstant()->GetValue();
          break;
        case HInstruction::kMul:
          new_val = cst1->AsFloatConstant()->GetValue() * cst2->AsFloatConstant()->GetValue();
          break;
        default:
          UNREACHABLE();
      }
      new_cst = graph->GetFloatConstant(new_val);
      return new_cst;
    }
    return nullptr;
}

static HConstant* GetConstant(HGraph* graph, DataType::Type type, int64_t value) {
  HConstant * cst = nullptr;
  if (DataType::IsIntegralType(type)) {
    cst = graph->GetConstant(type, value);
  } else if (type == DataType::Type::kFloat32) {
    float val = static_cast<float>(value);
    cst = graph->GetFloatConstant(val);
  } else if (type == DataType::Type::kFloat64) {
    double val = static_cast<double>(value);
    cst = graph->GetDoubleConstant(val);
  }
  return cst;
}

static HConstant* GetNegativeConstant(HGraph* graph, DataType::Type type, HConstant* input) {
  HConstant* cst = nullptr;
  if (DataType::IsIntegralType(type)) {
    int64_t val = Int64FromConstant(input);
    cst = GetConstant(graph, type, -1*val);
  } else if (type ==DataType::Type::kFloat32) {
    float val = input->AsFloatConstant()->GetValue();
    cst = GetConstant(graph, type, -1.0f*val);
  } else if (type == DataType::Type::kFloat64) {
    double val = input->AsDoubleConstant()->GetValue();
    cst = GetConstant(graph, type, -1.0f*val);
  }
  return cst;
}

HInstruction* GetOutOfLoopInput(HLoopInformation* loop, HPhi* phi) {
  HInputsRef inputs = phi->GetInputs();
  for (size_t i = 0; i < inputs.size(); ++i) {
    HInstruction* input = inputs[i];
    if (input->GetBlock()->GetLoopInformation() != loop) {
      return input;
    }
  }
  return nullptr;
}

bool IvStrengthReduction::IsCandidatePhi(HLoopInformation* loop, HPhi* phi) {
  //  (1) Phi must have two inputs
  //  (2) Loop condition must be based on this phi
  //  (3) Phi must be incremented by a constant amount
  HInputsRef inputs = phi->GetInputs();
  if (inputs.size() != 2 )
    return false;
  HInstruction* control = loop->GetHeader()->GetLastInstruction();
  HInductionVarAnalysis::InductionInfo* phi_info = induction_analysis_->LookupInfo(loop, phi);
  if (control->IsIf()) {
    HIf* ifs = control->AsIf();
    HInstruction* if_expr = ifs->InputAt(0);
    if (if_expr->IsCondition()) {
      HCondition* condition = if_expr->AsCondition();
      HInductionVarAnalysis::InductionInfo* a = induction_analysis_->LookupInfo(loop, condition->InputAt(0));
      HInductionVarAnalysis::InductionInfo* b = induction_analysis_->LookupInfo(loop, condition->InputAt(1));
      if (HInductionVarAnalysis::InductionEqual(a, phi_info) || HInductionVarAnalysis::InductionEqual(b, phi_info)) {
       // check if it is incremented by a constant amount
       ArenaSet<HInstruction*>* set = induction_analysis_->LookupCycle(phi);
       for (HInstruction* ins : *set) {
         if (ins->IsAdd()) {
           HBinaryOperation* op = ins->AsAdd();
           HConstant* cst = op->GetConstantRight();
           if (cst != nullptr) {
              biv_increment_ = op;
             return true;
           }
         }
       }
      }
    }
  }
  return false;
}

void IvStrengthReduction::PerformReduction(HLoopInformation* loop,
                                           HInstruction* derived_var,
                                           std::tuple<HInstruction*, HConstant*, HConstant*> val) {
  DCHECK(biv_increment_ != nullptr);
  HInstruction* var = std::get<0>(val);
  HPhi* phi = var->AsPhi();
  DCHECK(phi != nullptr);
  HInstruction* input = GetOutOfLoopInput(loop, phi);
  // Create two new instructions mul and add
  ArenaAllocator* allocator = derived_var->GetBlock()->GetGraph()->GetAllocator();
  HInstruction* new_mul =  new (allocator) HMul(derived_var->GetType(), input, std::get<1>(val), derived_var->GetDexPc());
  DCHECK(new_mul != nullptr);
  loop->GetPreHeader()->InsertInstructionBefore(new_mul, loop->GetPreHeader()->GetLastInstruction());
  HInstruction* new_add = new (allocator) HAdd(new_mul->GetType(), new_mul, std::get<2>(val), derived_var->GetDexPc());
  DCHECK(new_add != nullptr);
  loop->GetPreHeader()->InsertInstructionBefore(new_add, loop->GetPreHeader()->GetLastInstruction());
  HPhi* new_phi = new (allocator) HPhi(allocator, kNoRegNumber, 0, phi->GetType());
  DCHECK(new_phi != nullptr);
  phi->GetBlock()->InsertPhiAfter(new_phi, phi);
  new_phi->AddInput(new_add);
  HConstant* biv_inc_cst = biv_increment_->GetConstantRight();
  HConstant* biv_cst = GetNewConstant(HInstruction::InstructionKind::kMul, biv_inc_cst, std::get<1>(val));
  DCHECK(biv_cst != nullptr);
  HAdd* biv_add = new (allocator) HAdd(biv_inc_cst->GetType(), new_phi, biv_cst);
  DCHECK(biv_add != nullptr);
  biv_increment_->GetBlock()->InsertInstructionAfter(biv_add, biv_increment_);
  new_phi->AddInput(biv_add);
  // Replace uses of new add within the loop with new_phi
  const HUseList<HInstruction*>& uses = derived_var->GetUses();
  for (auto it = uses.begin(), end = uses.end(); it != end;) {
    HInstruction* user = it->GetUser();
    size_t input_index = it->GetIndex();
    ++it;
    user->ReplaceInput(new_phi, input_index);
  }
  const HUseList<HEnvironment*>& env_uses = derived_var->GetEnvUses();
  for (auto it = env_uses.begin(), end = env_uses.end(); it != end;) {
    HEnvironment* user = it->GetUser();
    size_t index = it->GetIndex();
    ++it;  // increment prior to potential removal
    user->RemoveAsUserOfInput(index);
    user->SetRawEnvAt(index, new_phi);
    new_phi->AddEnvUseAt(user, index);
  }
  DCHECK(!derived_var->HasUses());
}

bool IvStrengthReduction::IsCandidateForReduction(HLoopInformation* loop,
                                                  HPhi* biv,
                                                  HBinaryOperation* to_check,
                                                  ArenaSafeMap<HBinaryOperation*, triple>* candidate) {
  // Check if to_check is a derived induction variable
  // of the form biv*c1 + c2 where c1 and c2 are constants
  HInductionVarAnalysis::InductionInfo* ind_info = induction_analysis_->LookupInfo(loop, to_check);
  HInductionVarAnalysis::InductionInfo* biv_ind_info = induction_analysis_->LookupInfo(loop, biv);
  if (ind_info == nullptr)
    return false;
  DataType::Type type = to_check->GetType();
  HGraph* graph = to_check->GetBlock()->GetGraph();
  if (to_check->IsMul()) {
    HConstant* cst = to_check->GetConstantRight();
    if (cst == nullptr)
      return false;
    HInstruction* left_ins = to_check->InputAt(0);
    HInstruction* right_ins = to_check->InputAt(1);
    HInductionVarAnalysis::InductionInfo* left_ins_info = induction_analysis_->LookupInfo(loop, left_ins);
    HInductionVarAnalysis::InductionInfo* right_ins_info = induction_analysis_->LookupInfo(loop, right_ins);
    if (HInductionVarAnalysis::InductionEqual(biv_ind_info, left_ins_info)) {
      if (right_ins_info->induction_class == HInductionVarAnalysis::kInvariant) {
        HConstant * zero = GetConstant(graph, type, 0);
        candidate->Put(to_check, std::make_tuple(biv, cst, zero));
        return true;
      }
    }
    if (HInductionVarAnalysis::InductionEqual(biv_ind_info, right_ins_info) &&
        left_ins_info->induction_class == HInductionVarAnalysis::kInvariant) {
      HConstant* zero = GetConstant(graph, type, 0);
      candidate->Put(to_check, std::make_tuple(biv, cst, zero));
      return true;
    }
  } else if (to_check ->IsAdd() || to_check->IsSub()) {
    HInstruction* left = to_check->InputAt(0);
    HInstruction* right = to_check->InputAt(1);
    HConstant* cst = to_check->GetConstantRight();
    if (cst == nullptr)
     return false;
    if (to_check->IsSub()) {
     cst = GetNegativeConstant(graph, type, cst);
    }
    HInductionVarAnalysis::InductionInfo* left_ins_info = induction_analysis_->LookupInfo(loop, left);
    HInductionVarAnalysis::InductionInfo* right_ins_info = induction_analysis_->LookupInfo(loop, right);
    // Check for a derived induction variable
    if (!HInductionVarAnalysis::InductionEqual(left_ins_info, biv_ind_info) &&
        !HInductionVarAnalysis::InductionEqual(right_ins_info, biv_ind_info)) {
      bool found = false;
      for (auto it : *candidate) {
        HInductionVarAnalysis::InductionInfo* info = induction_analysis_->LookupInfo(loop, it.first);
        if (HInductionVarAnalysis::InductionEqual(left_ins_info, info) &&
            right_ins_info->induction_class == HInductionVarAnalysis::kInvariant) {
          HConstant * one = GetConstant(graph, type, 1);
          candidate->Put(to_check, std::make_tuple(it.first, one, cst));
          found = true;
          break;
        } else if (HInductionVarAnalysis::InductionEqual(right_ins_info, info)
                   && left_ins_info->induction_class == HInductionVarAnalysis::kInvariant) {
          HConstant* one = GetConstant(graph, type, 1);
          candidate->Put(to_check, std::make_tuple(it.first, one, cst));
          found = true;
          break;
        }
      }
      if (found == true)
        return true;
    }
  }
  return false;
}

bool IvStrengthReduction::SimplifyLoop(HLoopInformation* loop) {
  // for ( i; condition ; i +/- = cst ) {
  //   j = a*i + b;
  //   <.....>
  // }
  // with
  // s = a*i+b;
  // for ( i; condition ; i +/- = cst ) {
  //   j = s;
  //   <.....>
  //    s = s+a*c;
  // }
  bool did_reduction = false;
  HBasicBlock* header = loop->GetHeader();
  HGraph * graph = header->GetGraph();
  ArenaSafeMap <HBinaryOperation*, std::tuple<HInstruction*, HConstant*, HConstant*>> candidate(
     std::less<HBinaryOperation*>(), graph->GetAllocator()->Adapter(kArenaAllocInductionVarAnalysis));
  for (HInstructionIterator it(header->GetPhis()); !it.Done(); it.Advance()) {
    HInstruction* cur = it.Current();
    HPhi* phi = cur->AsPhi();
    if (IsCandidatePhi(loop, phi)) {
      for (HBlocksInLoopIterator it1(*loop); !it1.Done(); it1.Advance()) {
        HBasicBlock* block = it1.Current();
        if (block == header)
          continue;
        for (HInstructionIterator it2(block->GetInstructions()); !it2.Done(); it2.Advance()) {
          HInstruction* to_check = it2.Current();
          if (to_check->IsBinaryOperation())
            IsCandidateForReduction(loop, phi, to_check->AsBinaryOperation(), &candidate);
        }
      }
      for (auto itr = candidate.begin(); itr != candidate.end(); itr++) {
        HBinaryOperation* derived_var = itr->first;
        HInstruction::InstructionKind kind = derived_var->GetKind();
        triple val = itr->second;
        HInstruction* var = std::get<0>(val);
        if (var->GetId() != phi->GetId()) {
          HBinaryOperation* var_bin = var->AsBinaryOperation();
          if (derived_var->IsAdd() || derived_var->IsSub()) {
            DCHECK(var->IsBinaryOperation());
            HConstant* new_cst = GetNewConstant(kind, std::get<2>(val), std::get<2>(candidate.Get(var_bin)));
            DCHECK(new_cst != nullptr);
            std::get<0>(val) = std::get<0>(candidate.Get(var_bin));
            std::get<1>(val) = std::get<1>(candidate.Get(var_bin));
            std::get<2>(val) = new_cst;
            candidate.Overwrite(derived_var, val);
          } else if (derived_var->IsMul()) {
            HConstant* new_cst1 = GetNewConstant(kind, std::get<2>(val), std::get<1>(candidate.Get(var_bin)));
            HConstant* new_cst2 = GetNewConstant(kind, std::get<2>(val), std::get<2>(candidate.Get(var_bin)));
            DCHECK(new_cst1 != nullptr);
            DCHECK(new_cst2 != nullptr);
            std::get<0>(val) = std::get<0>(candidate.Get(var_bin));
            std::get<1>(val) = new_cst1;
            std::get<2>(val) = new_cst2;
            candidate.Overwrite(derived_var, val);
          }
        }
      }
      // Perform strength Reduction
      for (auto cur1 = candidate.begin(); cur1 != candidate.end(); cur1++) {
        PerformReduction(loop, cur1->first, cur1->second);
      }
    }
    // Clear the candidate map now
    if (!did_reduction && !candidate.empty()) {
      did_reduction = true;
      candidate.clear();
    }
  }
  return did_reduction;
}

}  // namespace art
