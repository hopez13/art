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

#include "escape.h"

#include "base/arena_allocator.h"
#include "nodes.h"

namespace art {

namespace {

struct SingletonState {
  bool is_singleton = false;
  bool is_singleton_and_not_returned = false;
  bool is_singleton_and_not_deopt_visible = false;

  void MarkAll(bool value) {
    is_singleton = value;
    is_singleton_and_not_returned = value;
    is_singleton_and_not_deopt_visible = value;
  }
};

// Does this instruction generate an alias for all of its inputs?
static bool IsInstructionAliasing(HInstruction* user,
                                  /*out*/ SingletonState* singleton_state) {
  if (user->IsBoundType() || user->IsNullCheck()) {
    // BoundType shouldn't normally be necessary for an allocation. Just be conservative
    // for the uncommon cases. Similarly, null checks are eventually eliminated for explicit
    // allocations, but if we see one before it is simplified, assume an alias.
    singleton_state->MarkAll(false);
    DCHECK_EQ(Primitive::kPrimVoid, user->GetType());
    return true;
  } else if (user->IsPhi() || user->IsSelect()) {
    // The reference is merged to HPhi/HSelect.
    // Hence, the reference is no longer the only name that can refer to its value.
    singleton_state->MarkAll(false);
    DCHECK_EQ(Primitive::kPrimVoid, user->GetType());
    return true;
  }

  /*
   * Note: A more in-depth analysis might assume the HInvoke return value
   * is also itself an alias for all the reference inputs.
   *
   * In our analysis, we assume HInvoke acts as an escape-to-heap, which is
   * even stronger than creating an alias, so we don't handle HInvoke here.
   */

  return false;
}

static bool IsInstructionAliasing(HInstruction* user) {
  SingletonState singleton_state;
  return IsInstructionAliasing(user, /*out*/ &singleton_state);
}

static bool IsReferenceAliasing(HInstruction* user, HInstruction* reference) {
  if (!IsInstructionAliasing(user)) {
    return false;
  }

  // Instructions that create aliases must be returning references.
  DCHECK_EQ(Primitive::kPrimNot, user->GetType());

  for (size_t input_count = 0; input_count < user->InputCount(); ++input_count) {
    if (user->InputAt(input_count) == reference) {
      return true;
    }
  }
  return false;
}

// Is 'reference' escaping to the heap via the 'user' instruction?
static bool IsReferenceEscapingToHeap(HInstruction* user,
                                      HInstruction* reference,
                                      /*out*/ SingletonState* singleton_state,
                                      bool ignore_reference = false) {
  // If `ignore_reference` is true, this is simply checking if
  // `user` is some kind of instruction that escapes a reference to the heap.
  //
  // Otherwise, we must match the input exactly.
  auto match_input = [=](size_t input) {
    return ignore_reference || (reference == user->InputAt(input));
  };

  if (user->IsInvoke() ||
      (user->IsInstanceFieldSet() && match_input(1)) ||
      (user->IsUnresolvedInstanceFieldSet() && match_input(1)) ||
      (user->IsStaticFieldSet() && match_input(1)) ||
      (user->IsUnresolvedStaticFieldSet() && match_input(0)) ||
      (user->IsArraySet() && match_input(2))) {
    // The reference is merged to HPhi/HSelect, passed to a callee, or stored to heap.
    // Hence, the reference is no longer the only name that can refer to its value.
    singleton_state->MarkAll(false);
    return true;
  } else if ((user->IsUnresolvedInstanceFieldGet() && match_input(0)) ||
               (user->IsUnresolvedInstanceFieldSet() && match_input(0))) {
    // The field is accessed in an unresolved way. We mark the object as a non-singleton.
    // Note that we could optimize this case and still perform some optimizations until
    // we hit the unresolved access, but the conservative assumption is the simplest.
    singleton_state->MarkAll(false);
    return true;
  }

  return false;
}

static bool IsReferenceEscapingToHeap(HInstruction* user,
                                      HInstruction* reference) {
  SingletonState singleton_state;
  return IsReferenceEscapingToHeap(user, reference, /*out*/ &singleton_state);
}

static bool IsInstructionEscapingToHeap(HInstruction *user) {
  SingletonState singleton_state;
  return IsReferenceEscapingToHeap(user,
                                   /* reference */ nullptr,
                                   /* out */ &singleton_state,
                                   /* ignore_reference */ true);
}

}  // namespace <anonymous>  // NOLINT

static void CalculateEscape(HInstruction* reference,
                            bool (*no_escape)(HInstruction*, HInstruction*),
                            SingletonState* singleton_state) {
  // For references not allocated in the method, don't assume anything.
  if (!reference->IsNewInstance() && !reference->IsNewArray()) {
    singleton_state->MarkAll(false);
    return;
  }
  // Assume the best until proven otherwise.
  singleton_state->MarkAll(true);

  // Visit all uses to determine if this reference can escape into the heap,
  // a method call, an alias, etc.
  for (const HUseListNode<HInstruction*>& use : reference->GetUses()) {
    HInstruction* user = use.GetUser();
    if (no_escape != nullptr && (*no_escape)(reference, user)) {
      // Client supplied analysis says there is no escape.
      continue;
    } else if (IsInstructionAliasing(user, /*out*/ singleton_state)) {
      return;
    } else if (IsReferenceEscapingToHeap(user,
                                         reference, /*out*/
                                         singleton_state)) {
      return;
    } else if (user->IsReturn()) {
      singleton_state->is_singleton_and_not_returned = false;
    }
  }

  // Look at the environment uses if it's for HDeoptimize. Other environment uses are fine,
  // as long as client optimizations that rely on this information are disabled for debuggable.
  for (const HUseListNode<HEnvironment*>& use : reference->GetEnvUses()) {
    HEnvironment* user = use.GetUser();
    if (user->GetHolder()->IsDeoptimize()) {
      singleton_state->is_singleton_and_not_deopt_visible = false;
      break;
    }
  }
}

void CalculateEscape(HInstruction* reference,
                     bool (*no_escape)(HInstruction*, HInstruction*),
                     /*out*/ bool* is_singleton,
                     /*out*/ bool* is_singleton_and_not_returned,
                     /*out*/ bool* is_singleton_and_not_deopt_visible) {
  SingletonState singleton_state;

  CalculateEscape(reference, no_escape, /*out*/&singleton_state);

  *is_singleton = singleton_state.is_singleton;
  *is_singleton_and_not_returned = singleton_state.is_singleton_and_not_returned;
  *is_singleton_and_not_deopt_visible = singleton_state.is_singleton_and_not_deopt_visible;
}

bool DoesNotEscape(HInstruction* reference, bool (*no_escape)(HInstruction*, HInstruction*)) {
  bool is_singleton = false;
  bool is_singleton_and_not_returned = false;
  bool is_singleton_and_not_deopt_visible = false;  // not relevant for escape
  CalculateEscape(reference,
                  no_escape,
                  &is_singleton,
                  &is_singleton_and_not_returned,
                  &is_singleton_and_not_deopt_visible);
  return is_singleton_and_not_returned;
}

EscapeVisitor::EscapeVisitor(HGraph* graph)
    : graph_(graph),
      escapee_list_(graph->GetArena()->Adapter(kArenaAllocLSE)) {
  UNUSED(graph_);
}

void EscapeVisitor::VisitBasicBlock(HBasicBlock* block) {
  DCHECK_EQ(0u, escapee_list_.size());

  for (HInstructionIterator it(block->GetPhis()); !it.Done(); it.Advance()) {
    VisitInstructionImpl(it.Current());
  }
  for (HInstructionIterator it(block->GetInstructions()); !it.Done(); it.Advance()) {
    VisitInstructionImpl(it.Current());
  }

  ClearEscapeeTracking();
}

void EscapeVisitor::AddEscapeeTracking(HInstruction* instruction) {
  // Only reference types can escape or be aliased.
  DCHECK_EQ(Primitive::kPrimNot, instruction->GetType());
  escapee_list_.push_back(instruction);
}

void EscapeVisitor::ClearEscapeeTracking() {
  escapee_list_.clear();
}

void EscapeVisitor::VisitInstructionImpl(HInstruction* instruction) {
  // Find extra aliases for existing references we are tracking.
  if (IsInstructionAliasing(instruction)) {  // Avoid iterating for non-aliases.
    const size_t escapee_count = escapee_list_.size();
    for (size_t i = 0; i < escapee_count; ++i) {  // Avoid iterating over newly-pushed escapees.
      HInstruction* escape_candidate = escapee_list_[i];
      if (IsReferenceAliasing(instruction, escape_candidate)) {
        escapee_list_.push_back(escape_candidate);
      }
    }
  }

  // VisitEscaped callback.
  if (IsInstructionEscapingToHeap(instruction)) {  // Avoid iterating for non-heap-escapes.
    bool clear_escapees = false;
    for (HInstruction* escapee : escapee_list_) {
      if (IsReferenceEscapingToHeap(instruction, escapee)) {
        // Call back to user-defined interface function.
        if (VisitEscaped(instruction, escapee)) {
          // Do not suppress any visits to escapees until all of them have been visited.
          clear_escapees = true;
        }
      }
    }

    if (clear_escapees) {
      ClearEscapeeTracking();
    }
  }

  // Call back to user-defined interface function.
  VisitInstruction(instruction);
}

}  // namespace art
