/*
 * Copyright (C) 2015 The Android Open Source Project
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

#include "load_store_elimination.h"

#include "base/arena_bit_vector.h"
#include "base/array_ref.h"
#include "base/bit_vector-inl.h"
#include "base/scoped_arena_allocator.h"
#include "base/scoped_arena_containers.h"
#include "escape.h"
#include "load_store_analysis.h"
#include "reference_type_propagation.h"

#include "scoped_thread_state_change-inl.h"

/**
 * The general algorithm of load-store elimination (LSE).
 * Load-store analysis in the previous pass collects a list of heap locations
 * and does alias analysis of those heap locations.
 * LSE keeps track of a list of heap values corresponding to the heap
 * locations. It visits basic blocks in reverse post order and for
 * each basic block, visits instructions sequentially, and processes
 * instructions as follows:
 * - If the instruction is a load, and the heap location for that load has a
 *   valid heap value, the load can be eliminated. In order to maintain the
 *   validity of all heap locations during the optimization phase, the real
 *   elimination is delayed till the end of LSE.
 * - If the instruction is a store, it updates the heap value for the heap
 *   location of the store with the store instruction. The real heap value
 *   can be fetched from the store instruction. Heap values are invalidated
 *   for heap locations that may alias with the store instruction's heap
 *   location. The store instruction can be eliminated unless the value stored
 *   is later needed e.g. by a load from the same/aliased heap location or
 *   the heap location persists at method return/deoptimization.
 *   The store instruction is also needed if it's not used to track the heap
 *   value anymore, e.g. when it fails to merge with the heap values from other
 *   predecessors.
 * - A store that stores the same value as the heap value is eliminated.
 * - The list of heap values are merged at basic block entry from the basic
 *   block's predecessors. The algorithm is single-pass, so loop side-effects is
 *   used as best effort to decide if a heap location is stored inside the loop.
 * - A special type of objects called singletons are instantiated in the method
 *   and have a single name, i.e. no aliases. Singletons have exclusive heap
 *   locations since they have no aliases. Singletons are helpful in narrowing
 *   down the life span of a heap location such that they do not always
 *   need to participate in merging heap values. Allocation of a singleton
 *   can be eliminated if that singleton is not used and does not persist
 *   at method return/deoptimization.
 * - For newly instantiated instances, their heap values are initialized to
 *   language defined default values.
 * - Some instructions such as invokes are treated as loading and invalidating
 *   all the heap values, depending on the instruction's side effects.
 * - Finalizable objects are considered as persisting at method
 *   return/deoptimization.
 * - SIMD graphs (with VecLoad and VecStore instructions) are also handled. Any
 *   partial overlap access among ArrayGet/ArraySet/VecLoad/Store is seen as
 *   alias and no load/store is eliminated in such case.
 * - Currently this LSE algorithm doesn't handle graph with try-catch, due to
 *   the special block merging structure.
 */

namespace art {

// Use HGraphDelegateVisitor for which all VisitInvokeXXX() delegate to VisitInvoke().
class LSEVisitor final : private HGraphDelegateVisitor {
 public:
  LSEVisitor(HGraph* graph,
             const HeapLocationCollector& heap_location_collector,
             OptimizingCompilerStats* stats);

  void VisitBasicBlock(HBasicBlock* block) override;

  // Remove recorded instructions that should be eliminated.
  void RemoveInstructions();

 private:
  static size_t CountSpecialMarkers(HGraph* graph,
                                    const HeapLocationCollector& heap_location_collector);

  LSEVisitor(HGraph* graph,
             const HeapLocationCollector& heap_location_collector,
             OptimizingCompilerStats* stats,
             size_t num_special_markers);

  HTypeConversion* AddTypeConversionIfNecessary(HInstruction* instruction,
                                                HInstruction* value,
                                                DataType::Type expected_type) {
    HTypeConversion* type_conversion = nullptr;
    // Should never add type conversion into boolean value.
    if (expected_type != DataType::Type::kBool &&
        !DataType::IsTypeConversionImplicit(value->GetType(), expected_type)) {
      type_conversion = new (GetGraph()->GetAllocator()) HTypeConversion(
          expected_type, value, instruction->GetDexPc());
      instruction->GetBlock()->InsertInstructionBefore(type_conversion, instruction);
    }
    return type_conversion;
  }

  // Find an instruction's substitute if it's a removed load.
  // Return the same instruction if it should not be removed.
  HInstruction* FindSubstitute(HInstruction* instruction) {
    if (!IsLoad(instruction)) {
      return instruction;
    }
    for (size_t i = 0u, size = removed_loads_.size(); i != size; ++i) {
      if (removed_loads_[i] == instruction) {
        HInstruction* substitute = substitute_instructions_for_loads_[i];
        // The substitute list is a flat hierarchy.
        DCHECK_EQ(FindSubstitute(substitute), substitute);
        return substitute;
      }
    }
    return instruction;
  }

  void AddRemovedLoad(HInstruction* load, HInstruction* heap_value) {
    if (verbose_) {
      LOG(ERROR) << "VMARKO: AddRemovedLoad " << load->DebugName() << "@" << load->GetDexPc()
          << " -> " << heap_value->DebugName() << "@" << heap_value->GetDexPc()
          << "/" << heap_value->GetId();
    }
    DCHECK(IsLoad(load));
    DCHECK_EQ(FindSubstitute(load), load);
    DCHECK_EQ(FindSubstitute(heap_value), heap_value) <<
        "Unexpected heap_value that has a substitute " << heap_value->DebugName();
    removed_loads_.push_back(load);
    substitute_instructions_for_loads_.push_back(heap_value);
  }

  // Scan the list of removed loads to see if we can reuse `type_conversion`, if
  // the other removed load has the same substitute and type and is dominated
  // by `type_conversion`.
  void TryToReuseTypeConversion(HInstruction* type_conversion, size_t index) {
    size_t size = removed_loads_.size();
    HInstruction* load = removed_loads_[index];
    HInstruction* substitute = substitute_instructions_for_loads_[index];
    for (size_t j = index + 1; j < size; j++) {
      HInstruction* load2 = removed_loads_[j];
      HInstruction* substitute2 = substitute_instructions_for_loads_[j];
      if (load2 == nullptr) {
        DCHECK(substitute2->IsTypeConversion());
        continue;
      }
      DCHECK(IsLoad(load2));
      DCHECK(substitute2 != nullptr);
      if (substitute2 == substitute &&
          load2->GetType() == load->GetType() &&
          type_conversion->GetBlock()->Dominates(load2->GetBlock()) &&
          // Don't share across irreducible loop headers.
          // TODO: can be more fine-grained than this by testing each dominator.
          (load2->GetBlock() == type_conversion->GetBlock() ||
           !GetGraph()->HasIrreducibleLoops())) {
        // The removed_loads_ are added in reverse post order.
        DCHECK(type_conversion->StrictlyDominates(load2));
        load2->ReplaceWith(type_conversion);
        load2->GetBlock()->RemoveInstruction(load2);
        removed_loads_[j] = nullptr;
        substitute_instructions_for_loads_[j] = type_conversion;
      }
    }
  }

  // InstructionOrMarker points either to an HInstruction or an entry in `special_markers_`.
  using InstructionOrMarker = void*;

  // An unknown heap value. Loads with such a value in the heap location cannot be eliminated.
  // A heap location can be set to unknown heap value when:
  // - it is coming from outside the method,
  // - it is killed due to aliasing or side effects or merging with an unknown value.
  InstructionOrMarker GetUnknownHeapValue() {
    return &special_markers_[0u];
  }

  // Default heap value after an allocation.
  // A heap location can be set to that value right after an allocation.
  InstructionOrMarker GetDefaultHeapValue() {
    return &special_markers_[1u];
  }

  bool IsSpecialMarker(InstructionOrMarker instruction) const {
    uintptr_t data = reinterpret_cast<uintptr_t>(special_markers_.data());
    uintptr_t offset = reinterpret_cast<uintptr_t>(instruction) - data;
    return offset < special_markers_.size() * sizeof(special_markers_[0]);
  }

  size_t SpecialMarkerIndex(InstructionOrMarker marker) const {
    DCHECK(IsSpecialMarker(marker));
    return static_cast<size_t>(reinterpret_cast<const uint32_t*>(marker) - special_markers_.data());
  }

  bool IsPhiPlaceholder(InstructionOrMarker instruction) {
    return IsSpecialMarker(instruction) &&
           instruction != GetUnknownHeapValue() &&
           instruction != GetDefaultHeapValue();
  }

  size_t GetPhiPlaceholderLocationIndex(InstructionOrMarker phi_placeholder, uint32_t block_id) {
    DCHECK(IsPhiPlaceholder(phi_placeholder));
    size_t special_marker_index = SpecialMarkerIndex(phi_placeholder);
    size_t phi_placeholders_begin = phi_placeholders_begin_for_block_[block_id];
    size_t location_index = special_marker_index - phi_placeholders_begin;
    DCHECK_LT(location_index, heap_location_collector_.GetNumberOfHeapLocations());
    return location_index;
  }

  uint32_t GetPhiPlaceholderBlockId(InstructionOrMarker phi_placeholder) {
    DCHECK(IsPhiPlaceholder(phi_placeholder));
    DCHECK_NE(phi_placeholder, GetUnknownHeapValue());
    DCHECK_NE(phi_placeholder, GetDefaultHeapValue());
    size_t special_marker_index = SpecialMarkerIndex(phi_placeholder);
    uint32_t block_id = special_markers_[special_marker_index];
    DCHECK_LT(block_id, GetGraph()->GetBlocks().size());
    DCHECK_GE(GetGraph()->GetBlocks()[block_id]->GetPredecessors().size(), 2u);
    return block_id;
  }

  HInstruction* AsInstruction(InstructionOrMarker instruction) {
    DCHECK(!IsSpecialMarker(instruction));
    return reinterpret_cast<HInstruction*>(instruction);
  }

  bool IsLoad(InstructionOrMarker instruction) {
    if (IsSpecialMarker(instruction)) {
      return false;
    }
    // Unresolved load is not treated as a load.
    HInstruction* insn = AsInstruction(instruction);
    return insn->IsInstanceFieldGet() ||
           insn->IsStaticFieldGet() ||
           insn->IsVecLoad() ||
           insn->IsArrayGet();
  }

  bool IsStore(InstructionOrMarker instruction) {
    if (IsSpecialMarker(instruction)) {
      return false;
    }
    // Unresolved store is not treated as a store.
    HInstruction* insn = AsInstruction(instruction);
    return insn->IsInstanceFieldSet() ||
           insn->IsArraySet() ||
           insn->IsVecStore() ||
           insn->IsStaticFieldSet();
  }

  // Check if it is allowed to use default values for the specified load.
  bool IsDefaultAllowedForLoad(HInstruction* instruction) {
    DCHECK(IsLoad(instruction));
    // Using defaults for VecLoads requires to create additional vector operations.
    // As there are some issues with scheduling vector operations it is better to avoid creating
    // them.
    return !instruction->IsVecOperation();
  }

  struct ValueDescription {
    InstructionOrMarker value;
    bool needs_loop_phi;
    InstructionOrMarker stored_by;
  };

  // Keep the store referenced by the instruction, or all stores that feed a Phi placeholder.
  // This is necessary if the stored heap value can be observed.
  void KeepStores(InstructionOrMarker instruction) {
    if (instruction == GetUnknownHeapValue()) {
      return;
    }
    if (IsSpecialMarker(instruction)) {
      DCHECK(IsPhiPlaceholder(instruction));
      if (verbose_) {
        uint32_t block_id = GetPhiPlaceholderBlockId(instruction);
        size_t idx = GetPhiPlaceholderLocationIndex(instruction, block_id);
        LOG(ERROR) << "VMARKO: Marking phi for keeping " << SpecialMarkerIndex(instruction)
            << " block: " << block_id << " idx: " << idx;
      }
      phi_placeholders_to_search_for_kept_stores_.SetBit(SpecialMarkerIndex(instruction));
    } else {
      DCHECK(IsStore(instruction));
      if (verbose_) {
        LOG(ERROR) << "VMARKO: Keeping store " << AsInstruction(instruction)->GetId();
      }
      kept_stores_.SetBit(AsInstruction(instruction)->GetId());
    }
  }

  // If a heap location X may alias with heap location at `loc_index`
  // and heap_values of that heap location X holds a store, keep that store.
  // It's needed for a dependent load that's not eliminated since any store
  // that may put value into the load's heap location needs to be kept.
  void KeepStoresIfAliasedToLocation(ScopedArenaVector<ValueDescription>& heap_values,
                                     size_t loc_index) {
    for (size_t i = 0u, size = heap_values.size(); i != size; ++i) {
      if (i == loc_index) {
        // We use this function when reading a location with unknown value and
        // therefore we cannot know what exact store wrote that unknown value.
        // But we can have a phi placeholder here marking stores to keep.
        DCHECK(IsSpecialMarker(heap_values[i].stored_by));
        KeepStores(heap_values[i].stored_by);
        heap_values[i].stored_by = GetUnknownHeapValue();
      } else if (heap_location_collector_.MayAlias(i, loc_index)) {
        KeepStores(heap_values[i].stored_by);
        heap_values[i].stored_by = GetUnknownHeapValue();
      }
    }
  }

  // `instruction` is being removed. Try to see if the null check on it
  // can be removed. This can happen if the same value is set in two branches
  // but not in dominators. Such as:
  //   int[] a = foo();
  //   if () {
  //     a[0] = 2;
  //   } else {
  //     a[0] = 2;
  //   }
  //   // a[0] can now be replaced with constant 2, and the null check on it can be removed.
  void TryRemovingNullCheck(HInstruction* instruction) {
    HInstruction* prev = instruction->GetPrevious();
    if ((prev != nullptr) && prev->IsNullCheck() && (prev == instruction->InputAt(0))) {
      // Previous instruction is a null check for this instruction. Remove the null check.
      prev->ReplaceWith(prev->InputAt(0));
      prev->GetBlock()->RemoveInstruction(prev);
    }
  }

  HInstruction* GetDefaultValue(DataType::Type type) {
    switch (type) {
      case DataType::Type::kReference:
        return GetGraph()->GetNullConstant();
      case DataType::Type::kBool:
      case DataType::Type::kUint8:
      case DataType::Type::kInt8:
      case DataType::Type::kUint16:
      case DataType::Type::kInt16:
      case DataType::Type::kInt32:
        return GetGraph()->GetIntConstant(0);
      case DataType::Type::kInt64:
        return GetGraph()->GetLongConstant(0);
      case DataType::Type::kFloat32:
        return GetGraph()->GetFloatConstant(0);
      case DataType::Type::kFloat64:
        return GetGraph()->GetDoubleConstant(0);
      default:
        UNREACHABLE();
    }
  }

  bool Equal(InstructionOrMarker heap_value, HInstruction* value) {
    DCHECK(value == FindSubstitute(value));
    DCHECK(!IsStore(value)) << value->DebugName();
    if (heap_value == GetUnknownHeapValue()) {
      // Don't compare GetUnknownHeapValue() with other values.
      return false;
    }
    if (heap_value == value) {
      return true;
    }
    if (heap_value == GetDefaultHeapValue() && IsZeroBitPattern(value)) {
      return true;
    }
    // Check if the `value` matches the substitute, which is an `HInstruction*`
    // and therefore we do not need to handle unknown or default value markers anymore.
    return !IsSpecialMarker(heap_value) && FindSubstitute(AsInstruction(heap_value)) == value;
  }

  bool CanValueBeKeptIfSameAsNew(InstructionOrMarker value,
                                 HInstruction* new_value,
                                 HInstruction* new_value_set_instr) {
    // For field/array set location operations, if the value is the same as the new_value
    // it can be kept even if aliasing happens. All aliased operations will access the same memory
    // range.
    // For vector values, this is not true. For example:
    //  packed_data = [0xA, 0xB, 0xC, 0xD];            <-- Different values in each lane.
    //  VecStore array[i  ,i+1,i+2,i+3] = packed_data;
    //  VecStore array[i+1,i+2,i+3,i+4] = packed_data; <-- We are here (partial overlap).
    //  VecLoad  vx = array[i,i+1,i+2,i+3];            <-- Cannot be eliminated because the value
    //                                                     here is not packed_data anymore.
    //
    // TODO: to allow such 'same value' optimization on vector data,
    // LSA needs to report more fine-grain MAY alias information:
    // (1) May alias due to two vector data partial overlap.
    //     e.g. a[i..i+3] and a[i+1,..,i+4].
    // (2) May alias due to two vector data may complete overlap each other.
    //     e.g. a[i..i+3] and b[i..i+3].
    // (3) May alias but the exact relationship between two locations is unknown.
    //     e.g. a[i..i+3] and b[j..j+3], where values of a,b,i,j are all unknown.
    // This 'same value' optimization can apply only on case (2).
    if (new_value_set_instr->IsVecOperation()) {
      return false;
    }

    return Equal(value, new_value);
  }

  ValueDescription PrepareLoopValue(HBasicBlock* block, size_t idx);
  void PrepareLoopValues(HBasicBlock* block);
  ValueDescription MergePredecessorValues(HBasicBlock* block, size_t idx);
  void MergePredecessorValues(HBasicBlock* block);

  void MaterializeNonLoopPhis(InstructionOrMarker phi_placeholder, size_t idx, DataType::Type type);
  InstructionOrMarker FindLoopPhisToMaterialize(InstructionOrMarker phi_placeholder,
                                                /*inout*/ArenaBitVector* visited);
  void MaterializeLoopPhis(const ArenaBitVector& current_dependencies,
                           const ScopedArenaVector<size_t>& phi_placeholder_indexes,
                           DataType::Type type);
  InstructionOrMarker TryToMaterializeLoopPhis(InstructionOrMarker phi_placeholder,
                                               DataType::Type type);
  void ProcessLoopPhiWithUnknownInput(InstructionOrMarker loop_phi_with_unknown_input);
  void ProcessLoadsRequiringLoopPhis();

  void VisitGetLocation(HInstruction* instruction, size_t idx);
  void VisitSetLocation(HInstruction* instruction, size_t idx, HInstruction* value);

  void VisitInstanceFieldGet(HInstanceFieldGet* instruction) override {
    HInstruction* object = instruction->InputAt(0);
    const FieldInfo& field = instruction->GetFieldInfo();
    VisitGetLocation(instruction, heap_location_collector_.GetFieldHeapLocation(object, &field));
  }

  void VisitInstanceFieldSet(HInstanceFieldSet* instruction) override {
    HInstruction* object = instruction->InputAt(0);
    const FieldInfo& field = instruction->GetFieldInfo();
    HInstruction* value = instruction->InputAt(1);
    size_t idx = heap_location_collector_.GetFieldHeapLocation(object, &field);
    VisitSetLocation(instruction, idx, value);
  }

  void VisitStaticFieldGet(HStaticFieldGet* instruction) override {
    HInstruction* cls = instruction->InputAt(0);
    const FieldInfo& field = instruction->GetFieldInfo();
    VisitGetLocation(instruction, heap_location_collector_.GetFieldHeapLocation(cls, &field));
  }

  void VisitStaticFieldSet(HStaticFieldSet* instruction) override {
    HInstruction* cls = instruction->InputAt(0);
    const FieldInfo& field = instruction->GetFieldInfo();
    HInstruction* value = instruction->InputAt(1);
    size_t idx = heap_location_collector_.GetFieldHeapLocation(cls, &field);
    VisitSetLocation(instruction, idx, value);
  }

  void VisitArrayGet(HArrayGet* instruction) override {
    VisitGetLocation(instruction, heap_location_collector_.GetArrayHeapLocation(instruction));
  }

  void VisitArraySet(HArraySet* instruction) override {
    size_t idx = heap_location_collector_.GetArrayHeapLocation(instruction);
    VisitSetLocation(instruction, idx, instruction->GetValue());
  }

  void VisitVecLoad(HVecLoad* instruction) override {
    VisitGetLocation(instruction, heap_location_collector_.GetArrayHeapLocation(instruction));
  }

  void VisitVecStore(HVecStore* instruction) override {
    size_t idx = heap_location_collector_.GetArrayHeapLocation(instruction);
    VisitSetLocation(instruction, idx, instruction->GetValue());
  }

  void VisitDeoptimize(HDeoptimize* instruction) override {
    ScopedArenaVector<ValueDescription>& heap_values =
        heap_values_for_[instruction->GetBlock()->GetBlockId()];
    for (size_t i = 0u, size = heap_values.size(); i != size; ++i) {
      ValueDescription& description = heap_values[i];
      if (description.stored_by == GetUnknownHeapValue()) {
        continue;
      }
      // Stores are generally observeable after deoptimization, except
      // for singletons that don't escape in the deoptimization environment.
      bool observable = true;
      ReferenceInfo* info = heap_location_collector_.GetHeapLocation(i)->GetReferenceInfo();
      if (info->IsSingleton()) {
        HInstruction* reference = info->GetReference();
        // Finalizable objects always escape.
        if (!reference->IsNewInstance() || !reference->AsNewInstance()->IsFinalizable()) {
          // Check whether the reference for a store is used by an environment local of
          // the HDeoptimize. If not, the singleton is not observed after deoptimization.
          const HUseList<HEnvironment*>& env_uses = reference->GetEnvUses();
          observable = std::any_of(
              env_uses.begin(),
              env_uses.end(),
              [instruction](const HUseListNode<HEnvironment*>& use) {
                return use.GetUser()->GetHolder() == instruction;
              });
        }
      }
      if (observable) {
        KeepStores(description.stored_by);
        description.stored_by = GetUnknownHeapValue();
      }
    }
  }

  // Keep necessary stores before exiting a method via return/throw.
  void HandleExit(HBasicBlock* block) {
    ScopedArenaVector<ValueDescription>& heap_values =
        heap_values_for_[block->GetBlockId()];
    if (verbose_) {
      LOG(ERROR) << "VMARKO: HandleExit in block #" << block->GetBlockId();
    }
    for (size_t i = 0u, size = heap_values.size(); i != size; ++i) {
      ReferenceInfo* ref_info = heap_location_collector_.GetHeapLocation(i)->GetReferenceInfo();
      if (!ref_info->IsSingletonAndRemovable()) {
        KeepStores(heap_values[i].stored_by);
        heap_values[i].stored_by = GetUnknownHeapValue();
      }
    }
  }

  void VisitReturn(HReturn* instruction) override {
    HandleExit(instruction->GetBlock());
  }

  void VisitReturnVoid(HReturnVoid* return_void) override {
    HandleExit(return_void->GetBlock());
  }

  void VisitThrow(HThrow* throw_instruction) override {
    HandleExit(throw_instruction->GetBlock());
  }

  void HandleInvoke(HInstruction* instruction) {
    SideEffects side_effects = instruction->GetSideEffects();
    ScopedArenaVector<ValueDescription>& heap_values =
        heap_values_for_[instruction->GetBlock()->GetBlockId()];
    for (size_t i = 0u, size = heap_values.size(); i != size; ++i) {
      ReferenceInfo* ref_info = heap_location_collector_.GetHeapLocation(i)->GetReferenceInfo();
      if (ref_info->IsSingleton()) {
        // Singleton references cannot be seen by the callee.
      } else {
        if (side_effects.DoesAnyRead()) {
          // Invocation may read the heap value.
          KeepStores(heap_values[i].stored_by);
          heap_values[i].stored_by = GetUnknownHeapValue();
        }
        if (side_effects.DoesAnyWrite()) {
          // Keep the store since it's not used to track the heap value anymore.
          KeepStores(heap_values[i].stored_by);
          heap_values[i] = GetUnknownHeapValueDescription();
        }
      }
    }
  }

  void VisitInvoke(HInvoke* invoke) override {
    HandleInvoke(invoke);
  }

  void VisitClinitCheck(HClinitCheck* clinit) override {
    // Class initialization check can result in class initializer calling arbitrary methods.
    HandleInvoke(clinit);
  }

  void VisitUnresolvedInstanceFieldGet(HUnresolvedInstanceFieldGet* instruction) override {
    // Conservatively treat it as an invocation.
    HandleInvoke(instruction);
  }

  void VisitUnresolvedInstanceFieldSet(HUnresolvedInstanceFieldSet* instruction) override {
    // Conservatively treat it as an invocation.
    HandleInvoke(instruction);
  }

  void VisitUnresolvedStaticFieldGet(HUnresolvedStaticFieldGet* instruction) override {
    // Conservatively treat it as an invocation.
    HandleInvoke(instruction);
  }

  void VisitUnresolvedStaticFieldSet(HUnresolvedStaticFieldSet* instruction) override {
    // Conservatively treat it as an invocation.
    HandleInvoke(instruction);
  }

  void VisitNewInstance(HNewInstance* new_instance) override {
    ReferenceInfo* ref_info = heap_location_collector_.FindReferenceInfoOf(new_instance);
    if (ref_info == nullptr) {
      // new_instance isn't used for field accesses. No need to process it.
      return;
    }
    if (ref_info->IsSingletonAndRemovable() && !new_instance->NeedsChecks()) {
      DCHECK(!new_instance->IsFinalizable());
      // new_instance can potentially be eliminated.
      singleton_new_instances_.push_back(new_instance);
    }
    ScopedArenaVector<ValueDescription>& heap_values =
        heap_values_for_[new_instance->GetBlock()->GetBlockId()];
    for (size_t i = 0u, size = heap_values.size(); i != size; ++i) {
      HInstruction* ref =
          heap_location_collector_.GetHeapLocation(i)->GetReferenceInfo()->GetReference();
      size_t offset = heap_location_collector_.GetHeapLocation(i)->GetOffset();
      if (ref == new_instance && offset >= mirror::kObjectHeaderSize) {
        // Instance fields except the header fields are set to default heap values.
        heap_values[i].value = GetDefaultHeapValue();
        DCHECK(!heap_values[i].needs_loop_phi);
        heap_values[i].stored_by = GetUnknownHeapValue();
      }
    }
  }

  void VisitNewArray(HNewArray* new_array) override {
    ReferenceInfo* ref_info = heap_location_collector_.FindReferenceInfoOf(new_array);
    if (ref_info == nullptr) {
      // new_array isn't used for array accesses. No need to process it.
      return;
    }
    if (ref_info->IsSingletonAndRemovable()) {
      if (new_array->GetLength()->IsIntConstant() &&
          new_array->GetLength()->AsIntConstant()->GetValue() >= 0) {
        // new_array can potentially be eliminated.
        singleton_new_instances_.push_back(new_array);
      } else {
        // new_array may throw NegativeArraySizeException. Keep it.
      }
    }
    ScopedArenaVector<ValueDescription>& heap_values =
        heap_values_for_[new_array->GetBlock()->GetBlockId()];
    for (size_t i = 0u, size = heap_values.size(); i != size; ++i) {
      HeapLocation* location = heap_location_collector_.GetHeapLocation(i);
      HInstruction* ref = location->GetReferenceInfo()->GetReference();
      if (ref == new_array && location->GetIndex() != nullptr) {
        // Array elements are set to default heap values.
        heap_values[i].value = GetDefaultHeapValue();
        DCHECK(!heap_values[i].needs_loop_phi);
        heap_values[i].stored_by = GetUnknownHeapValue();
      }
    }
  }

  ValueDescription GetUnknownHeapValueDescription() {
    return { /*value=*/ GetUnknownHeapValue(),
             /*needs_loop_phi=*/ false,
             /*stored_by=*/ GetUnknownHeapValue() };
  }

  void SearchPhiPlaceholdersForKeptStores();

  const HeapLocationCollector& heap_location_collector_;

  // Use local allocator for allocating memory.
  ScopedArenaAllocator allocator_;

  // Special markers:
  //   - unknown value
  //   - default value
  //   - Phi placeholders
  size_t num_special_markers_;
  ScopedArenaVector<uint32_t> special_markers_;

  // The start of the Phi placeholders in the `special_markers_`
  // for each block with multiple predecessors.
  ScopedArenaVector<size_t> phi_placeholders_begin_for_block_;

  // One array of heap value descriptions for each block.
  ScopedArenaVector<ScopedArenaVector<ValueDescription>> heap_values_for_;

  // We record the instructions that should be eliminated but may be
  // used by heap locations. They'll be removed in the end.
  ScopedArenaVector<HInstruction*> removed_loads_;
  ScopedArenaVector<HInstruction*> substitute_instructions_for_loads_;

  // We record loads and stores for re-processing when we find a loop Phi placeholder
  // with unknown value from a predecessor, and also for removing stores that are
  // found to be dead, i.e. not marked in `kept_stores_` at the end.
  struct LoadStoreRecord {
    HInstruction* load_or_store;
    size_t heap_location_index;
    HInstruction* stored_value;  // null for loads.
  };
  ScopedArenaVector<LoadStoreRecord> loads_and_stores_;
  // Record stores to keep in a bit vector indexed by instruction ID.
  ArenaBitVector kept_stores_;
  // When we need to keep all stores that feed a Phi placeholder, we just record the
  // index of that placeholder for processing after graph traversal.
  ArenaBitVector phi_placeholders_to_search_for_kept_stores_;

  // Loads that would require a loop Phi to replace are recorded for processing
  // later as we do not have enough information from back-edges to determine if
  // a suitable Phi can be found or created when we visit these loads.
  ScopedArenaHashMap<HInstruction*, ValueDescription> loads_requiring_loop_phi_;

  // Replacements for Phi placeholders requiring a loop Phi.
  // The unknown heap value is used to mark Phi placeholders that cannot be replaced.
  ScopedArenaVector<InstructionOrMarker> loop_phi_replacements_;

  ScopedArenaVector<HInstruction*> singleton_new_instances_;

  bool verbose_;

  DISALLOW_COPY_AND_ASSIGN(LSEVisitor);
};

size_t LSEVisitor::CountSpecialMarkers(HGraph* graph,
                                       const HeapLocationCollector& heap_location_collector) {
  size_t num_marker_phis = 0u;
  size_t num_heap_locations = heap_location_collector.GetNumberOfHeapLocations();
  for (HBasicBlock* block : graph->GetReversePostOrder()) {
    if (block->GetPredecessors().size() >= 2u) {
      num_marker_phis += num_heap_locations;
    }
  }
  return num_marker_phis + 2u;  // Unknown and default heap value.
}

LSEVisitor::LSEVisitor(HGraph* graph,
                       const HeapLocationCollector& heap_location_collector,
                       OptimizingCompilerStats* stats)
    : HGraphDelegateVisitor(graph, stats),
      heap_location_collector_(heap_location_collector),
      allocator_(graph->GetArenaStack()),
      num_special_markers_(CountSpecialMarkers(graph, heap_location_collector)),
      special_markers_(allocator_.Adapter(kArenaAllocLSE)),
      phi_placeholders_begin_for_block_(graph->GetBlocks().size(),
                                        allocator_.Adapter(kArenaAllocLSE)),
      heap_values_for_(graph->GetBlocks().size(),
                       ScopedArenaVector<ValueDescription>(allocator_.Adapter(kArenaAllocLSE)),
                       allocator_.Adapter(kArenaAllocLSE)),
      removed_loads_(allocator_.Adapter(kArenaAllocLSE)),
      substitute_instructions_for_loads_(allocator_.Adapter(kArenaAllocLSE)),
      loads_and_stores_(allocator_.Adapter(kArenaAllocLSE)),
      // We may add new instructions (default values, Phis) but we're not adding stores,
      // so we do not need the following BitVector to be expandable.
      kept_stores_(&allocator_,
                   /*start_bits=*/ graph->GetCurrentInstructionId(),
                   /*expandable=*/ false,
                   kArenaAllocLSE),
      phi_placeholders_to_search_for_kept_stores_(&allocator_,
                                                  num_special_markers_,
                                                  /*expandable=*/ false,
                                                  kArenaAllocLSE),
      loads_requiring_loop_phi_(allocator_.Adapter(kArenaAllocLSE)),
      loop_phi_replacements_(num_special_markers_, nullptr, allocator_.Adapter(kArenaAllocLSE)),
      singleton_new_instances_(allocator_.Adapter(kArenaAllocLSE)),
      verbose_(graph->PrettyMethod() ==
               "xjava.lang.Class[] java.lang.reflect.Proxy.intersectExceptions(java.lang.Class[], java.lang.Class[])") {
  // Clear bit vectors.
  phi_placeholders_to_search_for_kept_stores_.ClearAllBits();
  kept_stores_.ClearAllBits();
  // Reserve space for special markers and add markers for unknown and default value.
  special_markers_.reserve(num_special_markers_);
  special_markers_.push_back(static_cast<uint32_t>(-1));  // Unknown value.
  special_markers_.push_back(static_cast<uint32_t>(-1));  // Default value.
  if (verbose_) {
    LOG(ERROR) << "VMARKO: Verbose for " << graph->PrettyMethod();
  }
}

LSEVisitor::ValueDescription LSEVisitor::PrepareLoopValue(HBasicBlock* block, size_t idx) {
  HBasicBlock* pre_header = block->GetLoopInformation()->GetPreHeader();
  ScopedArenaVector<ValueDescription>& pre_header_heap_values =
      heap_values_for_[pre_header->GetBlockId()];
  size_t phi_placeholders_begin = phi_placeholders_begin_for_block_[block->GetBlockId()];
  // If the pre-header value is known and the heap location (the object or array and
  // array index) is a loop invariant, use a Phi placeholder for the value in the loop
  // header. If all predecessors are later found to have a known value, we can replace
  // loads from this location,  either with the pre-header value or with a new Phi.
  // Note that even for a static field access we require the LoadClass to be before
  // the loop; if the LoadClass is in the loop, the incoming value is unknown anyway.
  HeapLocation* location = heap_location_collector_.GetHeapLocation(idx);
  ReferenceInfo* ref_info = location->GetReferenceInfo();
  HInstruction* ref = ref_info->GetReference();
  HInstruction* index = location->GetIndex();
  InstructionOrMarker pre_header_value = pre_header_heap_values[idx].value;
  if (IsPhiPlaceholder(pre_header_value) &&
      loop_phi_replacements_[SpecialMarkerIndex(pre_header_value)] != nullptr) {
    pre_header_value = loop_phi_replacements_[SpecialMarkerIndex(pre_header_value)];
    DCHECK(pre_header_value != GetDefaultHeapValue());
  }
  bool needs_loop_phi =
      (pre_header_value != GetUnknownHeapValue()) &&
      (ref->GetBlock() != block && ref->GetBlock()->Dominates(block)) &&
      (index == nullptr || (index->GetBlock() != block && index->GetBlock()->Dominates(block)));
  InstructionOrMarker phi_placeholder = &special_markers_[phi_placeholders_begin + idx];
  InstructionOrMarker value = needs_loop_phi ? phi_placeholder : GetUnknownHeapValue();
  // Use the Phi placeholder for `stored_by` to make sure all incoming stores are kept
  // if the value in the location escapes. This is not applicable to singletons that are
  // defined inside the loop as they shall be dead in the loop header.
  bool stores_are_live =
      !ref_info->IsSingleton() || (ref->GetBlock() != block && ref->GetBlock()->Dominates(block));
  if (verbose_) {
    LOG(ERROR) << "VMARKO: loop loc[" << idx << "]: singleton=" << ref_info->IsSingleton()
        << " instruction: " << ref->DebugName() << "@" << ref->GetDexPc()
        << " ref_b: " << ref->GetBlock()->GetBlockId() << " b: " << block->GetBlockId()
        << " dominates: " << ref->GetBlock()->Dominates(block) << " live: " << stores_are_live;
  }
  InstructionOrMarker stored_by = stores_are_live ? phi_placeholder : GetUnknownHeapValue();
  return { value, needs_loop_phi, stored_by };
}

void LSEVisitor::PrepareLoopValues(HBasicBlock* block) {
  if (verbose_) {
    LOG(ERROR) << "VMARKO: PrepareLoopValues #" << block->GetBlockId()
        << " <-" << [&]() {
          std::ostringstream oss;
          for (HBasicBlock* p : block->GetPredecessors()) {
            oss << " " << p->GetBlockId();
          }
          return oss.str();
        }();
  }
  DCHECK(block->IsLoopHeader());
  int block_id = block->GetBlockId();
  HBasicBlock* pre_header = block->GetLoopInformation()->GetPreHeader();
  ScopedArenaVector<ValueDescription>& pre_header_heap_values =
      heap_values_for_[pre_header->GetBlockId()];
  size_t num_heap_locations = heap_location_collector_.GetNumberOfHeapLocations();
  DCHECK_EQ(num_heap_locations, pre_header_heap_values.size());
  ScopedArenaVector<ValueDescription>& heap_values = heap_values_for_[block_id];
  DCHECK(heap_values.empty());

  // Don't eliminate loads in irreducible loops.
  // Also keep the stores before the loop.
  if (block->GetLoopInformation()->IsIrreducible()) {
    heap_values.resize(num_heap_locations, GetUnknownHeapValueDescription());
    for (size_t i = 0u; i != num_heap_locations; ++i) {
      KeepStores(pre_header_heap_values[i].stored_by);
      pre_header_heap_values[i].stored_by = GetUnknownHeapValue();
    }
    return;
  }

  // Fill `heap_values` based on values from pre-header.
  heap_values.reserve(num_heap_locations);
  for (size_t idx = 0u; idx != num_heap_locations; ++idx) {
    heap_values.push_back(PrepareLoopValue(block, idx));
  }
  if (verbose_) {
    LOG(ERROR) << "VMARKO: PrepareLoopValues out #" << block->GetBlockId()
        << [&]() {
          std::ostringstream oss;
          for (size_t i = 0u; i != num_heap_locations; ++i) {
            oss << " loc[" << i << "]: " << (heap_values[i].stored_by == GetUnknownHeapValue())
                << "/" << IsSpecialMarker(heap_values[i].stored_by);
          }
          return oss.str();
        }();
  }
}

LSEVisitor::ValueDescription LSEVisitor::MergePredecessorValues(HBasicBlock* block, size_t idx) {
  ArrayRef<HBasicBlock* const> predecessors(block->GetPredecessors());
  DCHECK(!predecessors.empty());
  ValueDescription merged_description = heap_values_for_[predecessors[0]->GetBlockId()][idx];
  size_t phi_placeholders_begin = phi_placeholders_begin_for_block_[block->GetBlockId()];
  for (size_t i = 1u, size = predecessors.size(); i != size; ++i) {
    HBasicBlock* predecessor = predecessors[i];
    DCHECK(!heap_values_for_[predecessor->GetBlockId()].empty());
    ValueDescription pred_description = heap_values_for_[predecessor->GetBlockId()][idx];
    DCHECK(pred_description.value != nullptr);
    DCHECK(pred_description.stored_by != nullptr);
    if (IsPhiPlaceholder(pred_description.value) &&
        loop_phi_replacements_[SpecialMarkerIndex(pred_description.value)] != nullptr) {
      pred_description.value = loop_phi_replacements_[SpecialMarkerIndex(pred_description.value)];
      DCHECK(pred_description.value != GetDefaultHeapValue());
      pred_description.needs_loop_phi = false;
    }
    if (merged_description.value != GetUnknownHeapValue()) {
      if (pred_description.value == GetUnknownHeapValue()) {
        merged_description.value = GetUnknownHeapValue();
        merged_description.needs_loop_phi = false;
      } else if (pred_description.value != merged_description.value) {
        // There are conflicting known values. We may still be able to replace loads with a Phi.
        InstructionOrMarker phi_placeholder = &special_markers_[phi_placeholders_begin + idx];
        merged_description.value = phi_placeholder;
        // Propagate the need for a new loop Phi from all predecessors.
        merged_description.needs_loop_phi |= pred_description.needs_loop_phi;
      }
    }
    if (pred_description.stored_by != merged_description.stored_by) {
      // Use the Phi placeholder to track that we need to keep stores from all predecessors.
      InstructionOrMarker phi_placeholder = &special_markers_[phi_placeholders_begin + idx];
      merged_description.stored_by = phi_placeholder;
    }
  }
  return merged_description;
}

void LSEVisitor::MergePredecessorValues(HBasicBlock* block) {
  if (block->IsExitBlock()) {
    // Exit block doesn't really merge values since the control flow ends in
    // its predecessors. Each predecessor needs to make sure stores are kept
    // if necessary.
    return;
  }

  if (verbose_) {
    LOG(ERROR) << "VMARKO: MergePredecessorValues #" << block->GetBlockId()
        << " <-" << [&]() {
          std::ostringstream oss;
          for (HBasicBlock* p : block->GetPredecessors()) {
            oss << " " << p->GetBlockId();
          }
          size_t num = heap_location_collector_.GetNumberOfHeapLocations();
          for (size_t i = 0; i != num; ++i) {
            oss << " loc[" << i << "]: ";
            for (HBasicBlock* p : block->GetPredecessors()) {
              ScopedArenaVector<ValueDescription>& hvs = heap_values_for_[p->GetBlockId()];
              oss << " " << (hvs[i].stored_by == GetUnknownHeapValue())
                  << "/" << IsSpecialMarker(hvs[i].stored_by);
            }
          }
          return oss.str();
        }();
  }
  ScopedArenaVector<ValueDescription>& heap_values = heap_values_for_[block->GetBlockId()];
  DCHECK(heap_values.empty());
  size_t num_heap_locations = heap_location_collector_.GetNumberOfHeapLocations();
  if (block->GetPredecessors().empty()) {
    DCHECK(block->IsEntryBlock());
    heap_values.resize(num_heap_locations, GetUnknownHeapValueDescription());
    return;
  }

  heap_values.reserve(num_heap_locations);
  for (size_t i = 0u; i != num_heap_locations; ++i) {
    ValueDescription merged_description = MergePredecessorValues(block, i);
    if (kIsDebugBuild) {
      if (IsSpecialMarker(merged_description.value)) {
        if (merged_description.value != GetUnknownHeapValue() &&
            merged_description.value != GetDefaultHeapValue()) {
          uint32_t block_id = GetPhiPlaceholderBlockId(merged_description.value);
          GetGraph()->GetBlocks()[block_id]->Dominates(block);
        }
      } else {
        CHECK(AsInstruction(merged_description.value)->GetBlock()->Dominates(block));
      }
    }
    heap_values.push_back(merged_description);
  }
}

void LSEVisitor::VisitBasicBlock(HBasicBlock* block) {
  if (block->GetPredecessors().size() >= 2u) {
    // Create Phi placeholders referencing the block by the block ID.
    size_t num_heap_locations = heap_location_collector_.GetNumberOfHeapLocations();
    DCHECK_LE(num_heap_locations, special_markers_.capacity() - special_markers_.size());
    uint32_t block_id = block->GetBlockId();
    size_t phi_placeholders_begin = special_markers_.size();
    phi_placeholders_begin_for_block_[block_id] = phi_placeholders_begin;
    special_markers_.resize(phi_placeholders_begin + num_heap_locations, block_id);
    DCHECK_EQ(GetPhiPlaceholderBlockId(&special_markers_[phi_placeholders_begin]), block_id);
    DCHECK_EQ(GetPhiPlaceholderBlockId(&special_markers_.back()), block_id);
  }
  // Populate the heap_values array for this block.
  // TODO: try to reuse the heap_values array from one predecessor if possible.
  if (block->IsLoopHeader()) {
    PrepareLoopValues(block);
  } else {
    MergePredecessorValues(block);
  }
  // Visit instructions.
  HGraphVisitor::VisitBasicBlock(block);
}

static HInstruction* FindOrConstructNonLoopPhi(
    HBasicBlock* block,
    const ScopedArenaVector<HInstruction*>& phi_inputs,
    DataType::Type type) {
  for (HInstructionIterator phi_it(block->GetPhis()); !phi_it.Done(); phi_it.Advance()) {
    HInstruction* phi = phi_it.Current();
    DCHECK_EQ(phi->InputCount(), phi_inputs.size());
    auto cmp = [](HInstruction* lhs, const HUserRecord<HInstruction*>& rhs) {
      return lhs == rhs.GetInstruction();
    };
    if (std::equal(phi_inputs.begin(), phi_inputs.end(), phi->GetInputRecords().begin(), cmp)) {
      return phi;
    }
  }
  ArenaAllocator* allocator = block->GetGraph()->GetAllocator();
  HPhi* phi = new (allocator) HPhi(allocator, kNoRegNumber, phi_inputs.size(), type);
  for (size_t i = 0, size = phi_inputs.size(); i != size; ++i) {
    DCHECK_NE(phi_inputs[i]->GetType(), DataType::Type::kVoid) << phi_inputs[i]->DebugName();
    phi->SetRawInputAt(i, phi_inputs[i]);
  }
  block->AddPhi(phi);
  if (type == DataType::Type::kReference) {
    // Update reference type information. Pass invalid handles, these are not used for Phis.
    ReferenceTypePropagation rtp_fixup(block->GetGraph(),
                                       Handle<mirror::ClassLoader>(),
                                       Handle<mirror::DexCache>(),
                                       /* is_first_run= */ false);
    rtp_fixup.Visit(phi);
  }
  return phi;
}

void LSEVisitor::MaterializeNonLoopPhis(InstructionOrMarker phi_placeholder,
                                        size_t idx,
                                        DataType::Type type) {
  // Use local allocator to reduce peak memory usage.
  ScopedArenaAllocator allocator(allocator_.GetArenaStack());
  // Reuse the same vectors for collecting phi inputs.
  ScopedArenaVector<HInstruction*> phi_inputs(allocator.Adapter(kArenaAllocLSE));

  // Non-loop Phis cannot depent on loop Phi, so we should not see any loop header here.
  // And the only way for such merged value to reach a different heap location is through
  // a load at which point we materialize the Phi. Therefore all non-loop Phi placeholders
  // are tied to heap location `idx` an stored only in `heap_values_for_[.][idx]`.
  uint32_t block_id = GetPhiPlaceholderBlockId(phi_placeholder);
  DCHECK_EQ(idx, GetPhiPlaceholderLocationIndex(phi_placeholder, block_id));

  const ArenaVector<HBasicBlock*>& blocks = GetGraph()->GetBlocks();
  ScopedArenaVector<uint32_t> work_queue(allocator.Adapter(kArenaAllocLSE));
  work_queue.push_back(block_id);
  while (!work_queue.empty()) {
    uint32_t current_block_id = work_queue.back();
    HBasicBlock* current_block = blocks[current_block_id];
    DCHECK_GE(current_block->GetPredecessors().size(), 2u);
    DCHECK(!current_block->IsLoopHeader()) << GetGraph()->PrettyMethod();
    phi_inputs.clear();
    for (HBasicBlock* predecessor : current_block->GetPredecessors()) {
      InstructionOrMarker pred_value = heap_values_for_[predecessor->GetBlockId()][idx].value;
      if (IsPhiPlaceholder(pred_value) &&
          loop_phi_replacements_[SpecialMarkerIndex(pred_value)] != nullptr) {
        pred_value = loop_phi_replacements_[SpecialMarkerIndex(pred_value)];
        DCHECK(pred_value != GetDefaultHeapValue());
      }
      if (IsSpecialMarker(pred_value)) {
        DCHECK(pred_value != GetUnknownHeapValue());
        if (pred_value == GetDefaultHeapValue()) {
          phi_inputs.push_back(GetDefaultValue(type));
        } else {
          // We need to process the block where this phi placeholder originates.
          uint32_t phi_block_id = GetPhiPlaceholderBlockId(pred_value);
          DCHECK_EQ(idx, GetPhiPlaceholderLocationIndex(pred_value, phi_block_id));
          work_queue.push_back(phi_block_id);
        }
      } else {
        phi_inputs.push_back(AsInstruction(pred_value));
      }
    }
    if (phi_inputs.size() == current_block->GetPredecessors().size()) {
      // All inputs are available. Find or construct the Phi.
      HInstruction* replacement_phi = FindOrConstructNonLoopPhi(current_block, phi_inputs, type);
      size_t phi_placeholders_begin = phi_placeholders_begin_for_block_[current_block_id];
      InstructionOrMarker current_phi_placeholder = &special_markers_[phi_placeholders_begin + idx];
      loop_phi_replacements_[SpecialMarkerIndex(current_phi_placeholder)] = replacement_phi;
      // Remove the block from the queue.
      DCHECK_EQ(current_block_id, work_queue.back());
      work_queue.pop_back();
    }
  }
}

LSEVisitor::InstructionOrMarker LSEVisitor::FindLoopPhisToMaterialize(
    InstructionOrMarker phi_placeholder,
    /*inout*/ArenaBitVector* visited) {
  DCHECK(IsPhiPlaceholder(phi_placeholder));
  DCHECK_EQ(visited->NumSetBits(), 0u);
  DCHECK(loop_phi_replacements_[SpecialMarkerIndex(phi_placeholder)] == nullptr);

  // Use local allocator to reduce peak memory usage.
  ScopedArenaAllocator allocator(allocator_.GetArenaStack());
  ScopedArenaVector<InstructionOrMarker> work_queue(allocator.Adapter(kArenaAllocLSE));

  // Use depth first search to check if any non-Phi input is unknown.
  const ArenaVector<HBasicBlock*>& blocks = GetGraph()->GetBlocks();
  visited->SetBit(SpecialMarkerIndex(phi_placeholder));
  work_queue.push_back(phi_placeholder);
  while (!work_queue.empty()) {
    InstructionOrMarker current_phi_placeholder = work_queue.back();
    work_queue.pop_back();
    uint32_t current_block_id = GetPhiPlaceholderBlockId(current_phi_placeholder);
    HBasicBlock* current_block = blocks[current_block_id];
    DCHECK_GE(current_block->GetPredecessors().size(), 2u);
    size_t idx = GetPhiPlaceholderLocationIndex(current_phi_placeholder, current_block_id);
    if (verbose_) {
      LOG(ERROR) << "VMARKO: Searching phi " << SpecialMarkerIndex(current_phi_placeholder)
          << " block_id:" << current_block_id << " idx: " << idx;
    }
    for (HBasicBlock* predecessor : current_block->GetPredecessors()) {
      const ValueDescription& description = heap_values_for_[predecessor->GetBlockId()][idx];
      if (description.value == GetUnknownHeapValue()) {
        // We cannot create a Phi for this loop Phi placeholder.
        if (verbose_) {
          LOG(ERROR) << "VMARKO: Phi " << SpecialMarkerIndex(current_phi_placeholder)
              << " block_id:" << current_block_id << " idx: " << idx
              << " has unknown input from block " << predecessor->GetBlockId();
        }
        DCHECK(current_block->IsLoopHeader());
        return current_phi_placeholder;  // Report the loop Phi placeholder.
      }
      if (description.needs_loop_phi) {
        // Visit the predecessor Phi placeholder if it's not visited yet, unless it has
        // already been materialized and has a replacement in `loop_phi_replacements_`.
        DCHECK(IsPhiPlaceholder(description.value));
        if (!visited->IsBitSet(SpecialMarkerIndex(description.value)) &&
            loop_phi_replacements_[SpecialMarkerIndex(description.value)] == nullptr) {
          visited->SetBit(SpecialMarkerIndex(description.value));
          work_queue.push_back(description.value);
        }
      }
    }
  }

  // There are no unknown values feeding this Phi, so we can construct the Phis if needed.
  return nullptr;
}

void LSEVisitor::MaterializeLoopPhis(const ArenaBitVector& current_dependencies,
                                     const ScopedArenaVector<size_t>& phi_placeholder_indexes,
                                     DataType::Type type) {
  if (verbose_) {
    LOG(ERROR) << "VMARKO: Materializing " << current_dependencies.NumSetBits() << ":"
        << [&]() {
          std::ostringstream oss;
          for (uint32_t i : current_dependencies.Indexes()) {
            oss << " " << phi_placeholder_indexes[i] << "("
                << GetPhiPlaceholderBlockId(&special_markers_[phi_placeholder_indexes[i]]) << ")";
          }
          return oss.str();
        }();
  }
  // Materialize all predecessors that do not need a loop Phi and determine if all inputs
  // other than loop Phis are the same.
  const ArenaVector<HBasicBlock*>& blocks = GetGraph()->GetBlocks();
  InstructionOrMarker other_value = nullptr;
  for (uint32_t matrix_index : current_dependencies.Indexes()) {
    size_t phi_placeholder_index = phi_placeholder_indexes[matrix_index];
    InstructionOrMarker phi_placeholder = &special_markers_[phi_placeholder_index];
    uint32_t block_id = GetPhiPlaceholderBlockId(phi_placeholder);
    HBasicBlock* block = blocks[block_id];
    DCHECK_GE(block->GetPredecessors().size(), 2u);
    size_t idx = GetPhiPlaceholderLocationIndex(phi_placeholder, block_id);
    for (HBasicBlock* predecessor : block->GetPredecessors()) {
      ValueDescription& description = heap_values_for_[predecessor->GetBlockId()][idx];
      if (IsPhiPlaceholder(description.value) &&
          loop_phi_replacements_[SpecialMarkerIndex(description.value)] != nullptr) {
        description.value = loop_phi_replacements_[SpecialMarkerIndex(description.value)];
        DCHECK(description.value != GetDefaultHeapValue());
        description.needs_loop_phi = false;
      }
      if (!description.needs_loop_phi &&
          IsSpecialMarker(description.value) &&
          description.value != GetDefaultHeapValue()) {
        size_t pred_phi_placeholder_index = SpecialMarkerIndex(description.value);
        DCHECK(loop_phi_replacements_[pred_phi_placeholder_index] == nullptr);
        MaterializeNonLoopPhis(description.value, idx, type);
        DCHECK(loop_phi_replacements_[pred_phi_placeholder_index] != nullptr);
        description.value = loop_phi_replacements_[pred_phi_placeholder_index];
      }
      // Use the replacement value of previously replaced Phi placeholders.
      InstructionOrMarker value = description.needs_loop_phi
          ? loop_phi_replacements_[SpecialMarkerIndex(description.value)]
          : description.value;
      if (value != nullptr) {
        if (other_value == nullptr) {
          // The first other value we found.
          if (verbose_) {
            LOG(ERROR) << "VMARKO: Found first value "
                << (value == GetDefaultHeapValue() ? "DEFAULT" : AsInstruction(value)->DebugName());
          }
          other_value = value;
        } else if (other_value != GetUnknownHeapValue()) {
          // Check if the current `value` differs from the previous `other_value`.
          if (!(value == other_value) &&
              !(value == GetDefaultHeapValue() && IsZeroBitPattern(AsInstruction(other_value))) &&
              !(other_value == GetDefaultHeapValue() && IsZeroBitPattern(AsInstruction(value)))) {
            if (verbose_) {
              LOG(ERROR) << "VMARKO: Found different value "
                << (value == GetDefaultHeapValue() ? "DEFAULT" : AsInstruction(value)->DebugName());
            }
            other_value = GetUnknownHeapValue();
          }
        }
      }
    }
  }

  DCHECK(other_value != nullptr);
  if (other_value != GetUnknownHeapValue()) {
    HInstruction* replacement = (other_value == GetDefaultHeapValue()) ? GetDefaultValue(type)
                                                                       : AsInstruction(other_value);
    for (uint32_t matrix_index : current_dependencies.Indexes()) {
      size_t phi_placeholder_index = phi_placeholder_indexes[matrix_index];
      loop_phi_replacements_[phi_placeholder_index] = replacement;
      if (verbose_) {
        LOG(ERROR) << "VMARKO: Replacement: " << replacement->DebugName()
            << " " << (replacement->IsIntConstant() ? replacement->AsIntConstant()->GetValue() : 0)
            << " for placeholder " << phi_placeholder_index;
      }
    }
    return;
  }

  // There are different inputs to the Phi chain. Create the Phis.
  ArenaAllocator* allocator = GetGraph()->GetAllocator();
  for (uint32_t matrix_index : current_dependencies.Indexes()) {
    size_t phi_placeholder_index = phi_placeholder_indexes[matrix_index];
    InstructionOrMarker phi_placeholder = &special_markers_[phi_placeholder_index];
    uint32_t block_id = GetPhiPlaceholderBlockId(phi_placeholder);
    HBasicBlock* block = blocks[block_id];
    if (verbose_) {
      LOG(ERROR) << "VMARKO: New loop Phi";
    }
    loop_phi_replacements_[phi_placeholder_index] =
        new (allocator) HPhi(allocator, kNoRegNumber, block->GetPredecessors().size(), type);
  }
  // Fill the Phi inputs.
  for (uint32_t matrix_index : current_dependencies.Indexes()) {
    size_t phi_placeholder_index = phi_placeholder_indexes[matrix_index];
    InstructionOrMarker phi_placeholder = &special_markers_[phi_placeholder_index];
    uint32_t block_id = GetPhiPlaceholderBlockId(phi_placeholder);
    HBasicBlock* block = blocks[block_id];
    size_t idx = GetPhiPlaceholderLocationIndex(phi_placeholder, block_id);
    HInstruction* phi = AsInstruction(loop_phi_replacements_[phi_placeholder_index]);
    for (size_t i = 0, size = block->GetPredecessors().size(); i != size; ++i) {
      HBasicBlock* predecessor = block->GetPredecessors()[i];
      const ValueDescription& description = heap_values_for_[predecessor->GetBlockId()][idx];
      HInstruction* input;
      if (description.needs_loop_phi) {
        input = AsInstruction(loop_phi_replacements_[SpecialMarkerIndex(description.value)]);
        DCHECK(input != nullptr);
        DCHECK_NE(input->GetType(), DataType::Type::kVoid) << GetGraph()->PrettyMethod()
            << " " << input->DebugName() << "@" << input->GetDexPc();
      } else {
        input = (description.value == GetDefaultHeapValue()) ? GetDefaultValue(type)
                                                             : AsInstruction(description.value);
        DCHECK_NE(input->GetType(), DataType::Type::kVoid) << GetGraph()->PrettyMethod()
            << " " << input->DebugName() << "@" << input->GetDexPc();
      }
      phi->SetRawInputAt(i, input);
    }
  }
  // Add the Phis to their blocks.
  for (uint32_t matrix_index : current_dependencies.Indexes()) {
    size_t phi_placeholder_index = phi_placeholder_indexes[matrix_index];
    InstructionOrMarker phi_placeholder = &special_markers_[phi_placeholder_index];
    uint32_t block_id = GetPhiPlaceholderBlockId(phi_placeholder);
    HBasicBlock* block = blocks[block_id];
    block->AddPhi(AsInstruction(loop_phi_replacements_[phi_placeholder_index])->AsPhi());
  }
  if (type == DataType::Type::kReference) {
    ScopedArenaAllocator local_allocator(allocator_.GetArenaStack());
    ScopedArenaVector<HInstruction*> phis(local_allocator.Adapter(kArenaAllocLSE));
    for (uint32_t matrix_index : current_dependencies.Indexes()) {
      size_t phi_placeholder_index = phi_placeholder_indexes[matrix_index];
      phis.push_back(AsInstruction(loop_phi_replacements_[phi_placeholder_index]));
    }
    // Update reference type information. Pass invalid handles, these are not used for Phis.
    ReferenceTypePropagation rtp_fixup(GetGraph(),
                                       Handle<mirror::ClassLoader>(),
                                       Handle<mirror::DexCache>(),
                                       /* is_first_run= */ false);
    rtp_fixup.Visit(ArrayRef<HInstruction* const>(phis));
  }
}

LSEVisitor::InstructionOrMarker LSEVisitor::TryToMaterializeLoopPhis(
    InstructionOrMarker phi_placeholder,
    DataType::Type type) {
  DCHECK(loop_phi_replacements_[SpecialMarkerIndex(phi_placeholder)] == nullptr);

  // Use local allocator to reduce peak memory usage.
  ScopedArenaAllocator allocator(allocator_.GetArenaStack());

  // Find Phi placeholders to materialize.
  ArenaBitVector phi_placeholders_to_materialize(
      &allocator, num_special_markers_, /*expandable=*/ false, kArenaAllocLSE);
  phi_placeholders_to_materialize.ClearAllBits();
  InstructionOrMarker unknown_loop_phi =
      FindLoopPhisToMaterialize(phi_placeholder, &phi_placeholders_to_materialize);
  if (unknown_loop_phi != nullptr) {
    return unknown_loop_phi;
  }

  // Construct a matrix of loop phi placeholder dependencies. To reduce the memory usage,
  // assign new indexes to the Phi placeholders, making the matrix dense.
  ScopedArenaVector<size_t> matrix_indexes(num_special_markers_,
                                           static_cast<size_t>(-1),  // Invalid.
                                           allocator.Adapter(kArenaAllocLSE));
  ScopedArenaVector<size_t> phi_placeholder_indexes(allocator.Adapter(kArenaAllocLSE));
  size_t num_phi_placeholders = phi_placeholders_to_materialize.NumSetBits();
  phi_placeholder_indexes.reserve(num_phi_placeholders);
  for (uint32_t marker_index : phi_placeholders_to_materialize.Indexes()) {
    matrix_indexes[marker_index] = phi_placeholder_indexes.size();
    phi_placeholder_indexes.push_back(marker_index);
  }
  const ArenaVector<HBasicBlock*>& blocks = GetGraph()->GetBlocks();
  ScopedArenaVector<ArenaBitVector*> dependencies(allocator.Adapter(kArenaAllocLSE));
  dependencies.reserve(num_phi_placeholders);
  for (size_t matrix_index = 0; matrix_index != num_phi_placeholders; ++matrix_index) {
    static constexpr bool kExpandable = false;
    dependencies.push_back(
        ArenaBitVector::Create(&allocator, num_phi_placeholders, kExpandable, kArenaAllocLSE));
    ArenaBitVector* current_dependencies = dependencies.back();
    current_dependencies->ClearAllBits();
    current_dependencies->SetBit(matrix_index);  // Count the Phi placeholder as its own dependency.
    InstructionOrMarker current_phi_placeholder =
        &special_markers_[phi_placeholder_indexes[matrix_index]];
    uint32_t current_block_id = GetPhiPlaceholderBlockId(current_phi_placeholder);
    HBasicBlock* current_block = blocks[current_block_id];
    DCHECK_GE(current_block->GetPredecessors().size(), 2u);
    size_t idx = GetPhiPlaceholderLocationIndex(current_phi_placeholder, current_block_id);
    for (HBasicBlock* predecessor : current_block->GetPredecessors()) {
      const ValueDescription& pred_description = heap_values_for_[predecessor->GetBlockId()][idx];
      if (pred_description.needs_loop_phi) {
        DCHECK(IsPhiPlaceholder(pred_description.value));
        size_t pred_value_index = SpecialMarkerIndex(pred_description.value);
        DCHECK_EQ(loop_phi_replacements_[pred_value_index] != nullptr,
                  matrix_indexes[pred_value_index] == static_cast<size_t>(-1));
        if (matrix_indexes[pred_value_index] != static_cast<size_t>(-1)) {
          current_dependencies->SetBit(matrix_indexes[SpecialMarkerIndex(pred_description.value)]);
        }
      }
    }
  }

  // Use the Floyd-Warshall algorithm to determine all transitive dependencies.
  for (size_t k = 0; k != num_phi_placeholders; ++k) {
    for (size_t i = 0; i != num_phi_placeholders; ++i) {
      for (size_t j = 0; j != num_phi_placeholders; ++j) {
        if (dependencies[i]->IsBitSet(k) && dependencies[k]->IsBitSet(j)) {
          dependencies[i]->SetBit(j);
        }
      }
    }
  }

  // Count the number of transitive dependencies for each replacable Phi placeholder.
  ScopedArenaVector<size_t> num_dependencies(allocator.Adapter(kArenaAllocLSE));
  num_dependencies.reserve(num_phi_placeholders);
  for (size_t matrix_index = 0; matrix_index != num_phi_placeholders; ++matrix_index) {
    num_dependencies.push_back(dependencies[matrix_index]->NumSetBits());
  }

  // Pick a Phi placeholder with the smallest number of transitive dependencies and
  // materialize it and its dependencies. Repeat until we have materialized all.
  size_t remaining_phi_placeholders = num_phi_placeholders;
  while (remaining_phi_placeholders != 0u) {
    auto it = std::min_element(num_dependencies.begin(), num_dependencies.end());
    DCHECK_LE(*it, remaining_phi_placeholders);
    size_t current_matrix_index = std::distance(num_dependencies.begin(), it);
    ArenaBitVector* current_dependencies = dependencies[current_matrix_index];
    size_t current_num_dependencies = num_dependencies[current_matrix_index];
    MaterializeLoopPhis(*current_dependencies, phi_placeholder_indexes, type);
    for (uint32_t matrix_index = 0; matrix_index != num_phi_placeholders; ++matrix_index) {
      if (current_dependencies->IsBitSet(matrix_index)) {
        // Mark all dependencies as done by incrementing their `num_dependencies[.]`,
        // so that they shall never be the minimum again.
        num_dependencies[matrix_index] = num_phi_placeholders;
      } else if (dependencies[matrix_index]->IsBitSet(current_matrix_index)) {
        // Remove dependencies from other Phi placeholders.
        dependencies[matrix_index]->Subtract(current_dependencies);
        num_dependencies[matrix_index] -= current_num_dependencies;
      }
    }
    remaining_phi_placeholders -= current_num_dependencies;
  }

  // Report success.
  return nullptr;
}

// Re-process loads and stores in successors from the `loop_phi_with_unknown_input`. This may
// find one or more loads from `loads_requiring_loop_phi_` which cannot be replaced by Phis and
// propagate the load(s) as the new value(s) to successors; this may uncover new elimination
// opportunities. If we find no such load, we shall at least propagate an unknown value to some
// heap location that is needed by another loop Phi placeholder.
void LSEVisitor::ProcessLoopPhiWithUnknownInput(InstructionOrMarker loop_phi_with_unknown_input) {
  DCHECK(IsPhiPlaceholder(loop_phi_with_unknown_input));
  size_t loop_phi_with_unknown_input_index = SpecialMarkerIndex(loop_phi_with_unknown_input);
  DCHECK(loop_phi_replacements_[loop_phi_with_unknown_input_index] == nullptr);
  loop_phi_replacements_[loop_phi_with_unknown_input_index] = GetUnknownHeapValue();

  uint32_t block_id = GetPhiPlaceholderBlockId(loop_phi_with_unknown_input);
  if (verbose_) {
    LOG(ERROR) << "VMARKO: ProcessLoopPhiWithUnknownInput block_id: " << block_id
        << " idx: " << GetPhiPlaceholderLocationIndex(loop_phi_with_unknown_input, block_id);
  }
  const ArenaVector<HBasicBlock*> reverse_post_order = GetGraph()->GetReversePostOrder();
  size_t reverse_post_order_index = 0;
  size_t reverse_post_order_size = reverse_post_order.size();
  size_t loads_and_stores_index = 0u;
  size_t loads_and_stores_size = loads_and_stores_.size();

  // Skip blocks and instructions before the block containing the loop phi with unknown input.
  DCHECK_NE(reverse_post_order_index, reverse_post_order_size);
  while (reverse_post_order[reverse_post_order_index]->GetBlockId() != block_id) {
    HBasicBlock* block = reverse_post_order[reverse_post_order_index];
    while (loads_and_stores_index != loads_and_stores_size &&
           loads_and_stores_[loads_and_stores_index].load_or_store->GetBlock() == block) {
      ++loads_and_stores_index;
    }
    ++reverse_post_order_index;
    DCHECK_NE(reverse_post_order_index, reverse_post_order_size);
  }

  // Use local allocator to reduce peak memory usage.
  // ScopedArenaAllocator allocator(allocator_.GetArenaStack());  // FIXME
  // Reuse one temporary vector for all remaining blocks.
  size_t num_heap_locations = heap_location_collector_.GetNumberOfHeapLocations();
  ScopedArenaVector<ValueDescription> local_heap_values(allocator_.Adapter(kArenaAllocLSE));

  auto get_initial_value_description = [this](HBasicBlock* block, size_t idx) {
    ValueDescription description;
    if (block->IsLoopHeader()) {
      if (block->GetLoopInformation()->IsIrreducible()) {
        description = GetUnknownHeapValueDescription();
      } else {
        description = PrepareLoopValue(block, idx);
      }
    } else {
      description = MergePredecessorValues(block, idx);
    }
    if (IsPhiPlaceholder(description.value) &&
        loop_phi_replacements_[SpecialMarkerIndex(description.value)] != nullptr) {
      description.value = loop_phi_replacements_[SpecialMarkerIndex(description.value)];
      description.needs_loop_phi = false;
    }
    return description;
  };

  // Process remaining blocks and instructions.
  bool found_unreplacable_load = false;
  bool replaced_heap_value_with_unknown = false;
  for (; reverse_post_order_index != reverse_post_order_size; ++reverse_post_order_index) {
    HBasicBlock* block = reverse_post_order[reverse_post_order_index];
    if (block->IsExitBlock()) {
      continue;
    }

    // We shall reconstruct only the heap values that we need for processing loads and stores.
    local_heap_values.clear();
    local_heap_values.resize(
        num_heap_locations,
        { /*value=*/ nullptr, /*needs_loop_phi=*/ false, /*stored_by=*/ nullptr });

    for (; loads_and_stores_index != loads_and_stores_size; ++loads_and_stores_index) {
      HInstruction* load_or_store = loads_and_stores_[loads_and_stores_index].load_or_store;
      size_t idx = loads_and_stores_[loads_and_stores_index].heap_location_index;
      HInstruction* stored_value = loads_and_stores_[loads_and_stores_index].stored_value;
      DCHECK_EQ(load_or_store->GetSideEffects().DoesAnyWrite(), stored_value != nullptr);
      if (load_or_store->GetBlock() != block) {
        break;  // End of instructions from the current block.
      }
      auto it = loads_requiring_loop_phi_.find(
          stored_value != nullptr ? stored_value : load_or_store);
      if (it == loads_requiring_loop_phi_.end()) {
        if (verbose_) {
          LOG(ERROR) << "VMARKO: Skip " << load_or_store->DebugName()
              << "@" << load_or_store->GetDexPc();
        }
        continue;  // This load or store never needed a loop Phi.
      }
      if (verbose_) {
        LOG(ERROR) << "VMARKO: Process " << load_or_store->DebugName()
            << "@" << load_or_store->GetDexPc() << " idx: " << idx
            << "pred values:" << [&]() {
              std::ostringstream oss;
              for (HBasicBlock* p : block->GetPredecessors()) {
                const ValueDescription& d = heap_values_for_[p->GetBlockId()][idx];
                oss << " " << (d.value == GetUnknownHeapValue());
              }
              return oss.str();
            }();
      }
      const ValueDescription& description = it->second;
      if (stored_value == nullptr) {
        // Process the load unless it has previously been marked unreplacable.
        if (description.needs_loop_phi) {
          if (local_heap_values[idx].value == nullptr) {
            local_heap_values[idx] = get_initial_value_description(block, idx);
          }
          DCHECK(local_heap_values[idx].value != GetDefaultHeapValue());
          if (local_heap_values[idx].value == GetUnknownHeapValue()) {
            // This load cannot be replaced. Keep stores that feed the Phi placeholder
            // (no aliasing since then, otherwise the Phi placeholder would not have been
            // propagated as a value to this load) and store it as the new heap value.
            if (verbose_) {
              LOG(ERROR) << "VMARKO: Unreplacable: " << load_or_store->DebugName() << "@"
                  << load_or_store->GetDexPc();
            }
            found_unreplacable_load = true;
            DCHECK(IsPhiPlaceholder(description.value));
            KeepStores(description.value);
            local_heap_values[idx].value = load_or_store;
            DCHECK(!local_heap_values[idx].needs_loop_phi);
            loop_phi_replacements_[SpecialMarkerIndex(description.value)] = GetUnknownHeapValue();
          } else if (local_heap_values[idx].needs_loop_phi) {
            // The load may still be replaced with a Phi later.
            DCHECK_EQ(local_heap_values[idx].value, description.value);
          } else {
            // This load can be eliminated but we may need to construct non-loop Phis.
            if (IsSpecialMarker(local_heap_values[idx].value)) {
              if (verbose_) {
                LOG(ERROR) << "VMARKO: [x] Replacing IGET in block " << block_id
                    << " predecessors: " << load_or_store->GetBlock()->GetPredecessors().size();
              }
              size_t phi_placeholder_index = SpecialMarkerIndex(local_heap_values[idx].value);
              DCHECK(loop_phi_replacements_[phi_placeholder_index] == nullptr);
              MaterializeNonLoopPhis(local_heap_values[idx].value, idx, load_or_store->GetType());
              DCHECK(loop_phi_replacements_[phi_placeholder_index] != nullptr);
              local_heap_values[idx].value = loop_phi_replacements_[phi_placeholder_index];
            }
            HInstruction* old_substitute = FindSubstitute(load_or_store);
            HInstruction* heap_value = AsInstruction(local_heap_values[idx].value);
            if (old_substitute != load_or_store) {
              DCHECK(old_substitute == heap_value) << load_or_store->DebugName() << "@"
                  << load_or_store->GetDexPc() << " -> " << old_substitute->DebugName() << "@"
                  << old_substitute->GetDexPc() << "/" << old_substitute->GetId() << " v. "
                  << heap_value->DebugName() << "@" << heap_value->GetDexPc() << "/"
                  << heap_value->GetId();
            } else {
              DCHECK_EQ(heap_value, FindSubstitute(heap_value));
              if (verbose_) {
                LOG(ERROR) << "VMARKO: Adding substitute for " << load_or_store->DebugName() << "@"
                    << load_or_store->GetDexPc() << " -> " << old_substitute->DebugName()
                    << " with old " << old_substitute->DebugName() << "@"
                    << old_substitute->GetDexPc() << "/" << old_substitute->GetId();
              }
              AddRemovedLoad(load_or_store, AsInstruction(heap_value));
              TryRemovingNullCheck(load_or_store);
            }
          }
        }
      } else {
        // Process the store by updating `local_heap_values[idx]`. The last update shall
        // shall be propagated to `heap_values[idx]` if it previously needed a loop Phi
        // at the end of the block.
        InstructionOrMarker replacement =
            loop_phi_replacements_[SpecialMarkerIndex(description.value)];
        DCHECK(replacement != GetDefaultHeapValue());
        if (replacement == nullptr) {
          // No replacement yet, use the Phi placeholder from the load.
          local_heap_values[idx].value = description.value;
          DCHECK(description.needs_loop_phi);
          local_heap_values[idx].needs_loop_phi = true;
        } else {
          // Use the replacement if any as the the value, otherwise use the load.
          local_heap_values[idx].value =
              (replacement != GetUnknownHeapValue()) ? replacement : stored_value;
          local_heap_values[idx].needs_loop_phi = false;
        }
        local_heap_values[idx].stored_by = load_or_store;
      }
    }

    // All heap values that previously needed a loop Phi at the end of the block
    // need to be updated for processing successors.
    ScopedArenaVector<ValueDescription>& heap_values = heap_values_for_[block->GetBlockId()];
    for (size_t idx = 0; idx != num_heap_locations; ++idx) {
      if (heap_values[idx].needs_loop_phi) {
        DCHECK(IsPhiPlaceholder(heap_values[idx].value));
        if (local_heap_values[idx].value != nullptr) {
          heap_values[idx] = local_heap_values[idx];
        } else {
          heap_values[idx] = get_initial_value_description(block, idx);
        }
        if (heap_values[idx].value == GetUnknownHeapValue()) {
          replaced_heap_value_with_unknown = true;
        }
      }
      if (verbose_ && idx == 1u) {
        LOG(ERROR) << "Updated heap_values[" << idx << "] in block #" << block->GetBlockId()
            << ": " << heap_values[idx].needs_loop_phi
            << " ~ " << (local_heap_values[idx].value == GetUnknownHeapValue())
            << " -> " << (heap_values[idx].value == GetUnknownHeapValue())
            << " p:" << [&]() {
              std::ostringstream oss;
              for (HBasicBlock* p : block->GetPredecessors()) {
                const ValueDescription& d = heap_values_for_[p->GetBlockId()][idx];
                oss << " " << (d.value == GetUnknownHeapValue());
                if (IsPhiPlaceholder(d.value) &&
                    loop_phi_replacements_[SpecialMarkerIndex(d.value)] != nullptr) {
                  oss << "x" << (loop_phi_replacements_[SpecialMarkerIndex(d.value)] ==
                                     GetUnknownHeapValue());
                }
              }
              return oss.str();
            }();
      }
    }
  }
  DCHECK(found_unreplacable_load || replaced_heap_value_with_unknown) << GetGraph()->PrettyMethod();
}

void LSEVisitor::ProcessLoadsRequiringLoopPhis() {
  for (auto& entry : loads_requiring_loop_phi_) {
    HInstruction* load = entry.first;
    if (verbose_) {
      LOG(ERROR) << "VMARKO: Processing load " << load->DebugName() << "@" << load->GetDexPc()
          << "/" << load->GetId();
    }
    const ValueDescription& description = entry.second;
    size_t phi_placeholder_index = SpecialMarkerIndex(description.value);
    while (loop_phi_replacements_[phi_placeholder_index] == nullptr) {
      InstructionOrMarker loop_phi_with_unknown_input =
          TryToMaterializeLoopPhis(description.value, load->GetType());
      if (loop_phi_with_unknown_input != nullptr) {
        DCHECK(loop_phi_replacements_[phi_placeholder_index] == nullptr);
        ProcessLoopPhiWithUnknownInput(loop_phi_with_unknown_input);
      } else {
        DCHECK(loop_phi_replacements_[phi_placeholder_index] != nullptr);
      }
    }
    DCHECK(loop_phi_replacements_[phi_placeholder_index] != nullptr);
    if (loop_phi_replacements_[phi_placeholder_index] == GetUnknownHeapValue()) {
      KeepStores(description.stored_by);
    } else {
      HInstruction* heap_value =
          AsInstruction(loop_phi_replacements_[SpecialMarkerIndex(description.value)]);
      HInstruction* old_substitute = FindSubstitute(load);
      if (old_substitute != load) {
        DCHECK(old_substitute == heap_value) << load->DebugName() << "@"
            << load->GetDexPc() << " -> " << old_substitute->DebugName() << "@"
            << old_substitute->GetDexPc() << "/" << old_substitute->GetId() << " v. "
            << heap_value->DebugName() << "@" << heap_value->GetDexPc() << "/"
            << heap_value->GetId();
      } else {
        DCHECK_EQ(heap_value, FindSubstitute(heap_value));
        if (verbose_) {
          LOG(ERROR) << "VMARKO: [x] Adding substitute for " << load->DebugName() << "@"
              << load->GetDexPc() << " -> " << old_substitute->DebugName()
              << " with old " << old_substitute->DebugName() << "@"
              << old_substitute->GetDexPc() << "/" << old_substitute->GetId();
        }
        AddRemovedLoad(load, AsInstruction(heap_value));
        TryRemovingNullCheck(load);
      }
    }
  }
}

void LSEVisitor::VisitGetLocation(HInstruction* instruction, size_t idx) {
  DCHECK_NE(idx, HeapLocationCollector::kHeapLocationNotFound);
  uint32_t block_id = instruction->GetBlock()->GetBlockId();
  ScopedArenaVector<ValueDescription>& heap_values = heap_values_for_[block_id];
  ValueDescription& description = heap_values[idx];
  if (IsPhiPlaceholder(description.value) &&
      loop_phi_replacements_[SpecialMarkerIndex(description.value)] != nullptr) {
    description.value = loop_phi_replacements_[SpecialMarkerIndex(description.value)];
    DCHECK(!IsPhiPlaceholder(description.value));
    DCHECK(description.value != GetDefaultHeapValue());
  }
  if (verbose_) {
    LOG(ERROR) << "VMARKO: VisitGetLocation "
        << instruction->DebugName() << "@" << instruction->GetDexPc() << "/" << instruction->GetId()
        << " " << idx
        << " " << (description.value == GetDefaultHeapValue()) << "/"
        << (description.value == GetUnknownHeapValue()) << "/"
        << IsPhiPlaceholder(description.value);
  }
  loads_and_stores_.push_back({ instruction, idx, /*stored_value=*/ nullptr });
  if (description.value == GetDefaultHeapValue()) {
    DCHECK(!description.needs_loop_phi);
    DCHECK_EQ(description.stored_by, GetUnknownHeapValue());
    if (IsDefaultAllowedForLoad(instruction)) {
      HInstruction* constant = GetDefaultValue(instruction->GetType());
      AddRemovedLoad(instruction, constant);
      description.value = constant;
      return;
    } else {
      description.value = GetUnknownHeapValue();
    }
  }
  if (description.value == GetUnknownHeapValue()) {
    // Load isn't eliminated. Put the load as the value into the HeapLocation.
    // This acts like GVN but with better aliasing analysis.
    DCHECK(!description.needs_loop_phi);
    description.value = instruction;
    KeepStoresIfAliasedToLocation(heap_values, idx);
  } else if (description.needs_loop_phi) {
    // We do not know yet if the value is known for all back edges. Record for future processing.
    DCHECK(IsPhiPlaceholder(description.value));
    loads_requiring_loop_phi_.insert(std::make_pair(instruction, description));
  } else {
    // This load can be eliminated but we may need to construct non-loop Phis.
    if (IsSpecialMarker(description.value)) {
      if (verbose_) {
        LOG(ERROR) << "VMARKO: Replacing IGET in block " << block_id
            << " predecessors: " << instruction->GetBlock()->GetPredecessors().size();
      }
      size_t phi_placeholder_index = SpecialMarkerIndex(description.value);
      DCHECK(loop_phi_replacements_[phi_placeholder_index] == nullptr);
      MaterializeNonLoopPhis(description.value, idx, instruction->GetType());
      DCHECK(loop_phi_replacements_[phi_placeholder_index] != nullptr);
      description.value = loop_phi_replacements_[phi_placeholder_index];
    }
    HInstruction* heap_value = FindSubstitute(AsInstruction(description.value));
    AddRemovedLoad(instruction, AsInstruction(heap_value));
    TryRemovingNullCheck(instruction);
  }
}

void LSEVisitor::VisitSetLocation(HInstruction* instruction, size_t idx, HInstruction* value) {
  if (verbose_) {
    LOG(ERROR) << "VMARKO: VisitSetLocation "
        << instruction->DebugName() << "@" << instruction->GetDexPc() << "/" << instruction->GetId()
        << " " << idx;
  }
  DCHECK_NE(idx, HeapLocationCollector::kHeapLocationNotFound);
  DCHECK(!IsStore(value)) << value->DebugName();
  // value may already have a substitute.
  value = FindSubstitute(value);
  HBasicBlock* block = instruction->GetBlock();
  ScopedArenaVector<ValueDescription>& heap_values = heap_values_for_[block->GetBlockId()];
  ValueDescription& description = heap_values[idx];

  if (Equal(description.value, value)) {
    // Store into the heap location with the same value.
    // This store can be eliminated right away.
    block->RemoveInstruction(instruction);
    return;
  }

  // Update description.
  auto it = loads_requiring_loop_phi_.find(value);
  if (it != loads_requiring_loop_phi_.end()) {
    // Propapate the Phi placeholder to the description.
    description.value = it->second.value;
    description.needs_loop_phi = true;
  } else {
    description.value = value;
    description.needs_loop_phi = false;
  }

  // If the `description.stored_by` specified a store from this block, it shall be removed
  // at the end; it cannot be marked for keeping in `kept_stores_` anymore
  // after we update the `description.stored_by` below.
  DCHECK(IsSpecialMarker(description.stored_by) ||
         AsInstruction(description.stored_by)->GetBlock() != block ||
         !kept_stores_.IsBitSet(AsInstruction(description.stored_by)->GetId()));

  loads_and_stores_.push_back({ instruction, idx, value });
  if (instruction->CanThrow()) {
    // We cannot remove a possibly throwing store. Therefore we do not need to track it
    // in the value description's `stored_by`. However, previous stores can become visible.
    // TODO: Add a test for this.
    kept_stores_.SetBit(instruction->GetId());
    KeepStores(description.stored_by);
    // TODO: It does not matter if we record the instruction in `stored_by`
    // as it's already marked as kept.
    description.stored_by = GetUnknownHeapValue();
  } else {
    if (verbose_) {
      LOG(ERROR) << "VMARKO: possibly removed store: "
          << instruction->DebugName() << "@" << instruction->GetDexPc() << " " << idx
          << " id: " << instruction->GetId();
    }
    // Track the store in the value description. If the value is loaded or needed after
    // return/deoptimization later, this store isn't really redundant.
    description.stored_by = instruction;
  }

  // This store may kill values in other heap locations due to aliasing.
  for (size_t i = 0u, size = heap_values.size(); i != size; ++i) {
    if (i == idx ||
        heap_values[i].value == GetUnknownHeapValue() ||
        CanValueBeKeptIfSameAsNew(heap_values[i].value, value, instruction) ||
        !heap_location_collector_.MayAlias(i, idx)) {
      continue;
    }
    // Kill heap locations that may alias and as a result if the heap value
    // is a store, the store needs to be kept.
    KeepStores(heap_values[i].stored_by);
    heap_values[i] = GetUnknownHeapValueDescription();
  }
}

void LSEVisitor::SearchPhiPlaceholdersForKeptStores() {
  ScopedArenaVector<uint32_t> work_queue(allocator_.Adapter(kArenaAllocLSE));
  size_t start_size = phi_placeholders_to_search_for_kept_stores_.NumSetBits();
  if (verbose_) {
    LOG(ERROR) << "VMARKO: Start size for Phi queue: " << start_size;
  }
  work_queue.reserve(((start_size * 3u) + 1u) / 2u);  // Reserve 1.5x start size, rounded up.
  for (uint32_t index : phi_placeholders_to_search_for_kept_stores_.Indexes()) {
    work_queue.push_back(index);
  }
  const ArenaVector<HBasicBlock*>& blocks = GetGraph()->GetBlocks();
  while (!work_queue.empty()) {
    InstructionOrMarker phi_placeholder = &special_markers_[work_queue.back()];
    work_queue.pop_back();
    uint32_t block_id = GetPhiPlaceholderBlockId(phi_placeholder);
    size_t idx = GetPhiPlaceholderLocationIndex(phi_placeholder, block_id);
    if (verbose_) {
      LOG(ERROR) << "VMARKO: Processing Phi placeholder for keeping "
          << block_id << " idx: " << idx;
    }
    HBasicBlock* block = blocks[block_id];
    for (HBasicBlock* predecessor : block->GetPredecessors()) {
      ScopedArenaVector<ValueDescription>& heap_values =
          heap_values_for_[predecessor->GetBlockId()];
      // For loop back-edges we must also preserve all stores to locations that may alias
      // with the location `idx`.
      // TODO: Review whether we need to keep stores to aliased locations from pre-header.
      // TODO: Add tests cases around this.
      bool is_back_edge =
          block->IsLoopHeader() && predecessor != block->GetLoopInformation()->GetPreHeader();
      size_t start = is_back_edge ? 0u : idx;
      size_t end = is_back_edge ? heap_values.size() : idx + 1u;
      for (size_t i = start; i != end; ++i) {
        InstructionOrMarker stored_by = heap_values[i].stored_by;
        if (stored_by != GetUnknownHeapValue() &&
            (i == idx || heap_location_collector_.MayAlias(i, idx))) {
          if (IsSpecialMarker(stored_by)) {
            DCHECK(IsPhiPlaceholder(stored_by));
            size_t phi_placeholder_index = SpecialMarkerIndex(stored_by);
            if (!phi_placeholders_to_search_for_kept_stores_.IsBitSet(phi_placeholder_index)) {
              phi_placeholders_to_search_for_kept_stores_.SetBit(phi_placeholder_index);
              work_queue.push_back(phi_placeholder_index);
            }
          } else {
            DCHECK(IsStore(stored_by));
            if (verbose_) {
              LOG(ERROR) << "VMARKO: Keeping store for Phi " << AsInstruction(stored_by)->GetId();
            }
            kept_stores_.SetBit(AsInstruction(stored_by)->GetId());
          }
        }
      }
    }
  }
}

void LSEVisitor::RemoveInstructions() {
  ProcessLoadsRequiringLoopPhis();

  size_t size = removed_loads_.size();
  DCHECK_EQ(size, substitute_instructions_for_loads_.size());
  for (size_t i = 0; i != size; ++i) {
    HInstruction* load = removed_loads_[i];
    if (load == nullptr) {
      // The load has been handled in the scan for type conversion below.
      DCHECK(substitute_instructions_for_loads_[i]->IsTypeConversion());
      continue;
    }
    DCHECK(IsLoad(load));
    DCHECK(load->GetBlock() != nullptr) << load->DebugName() << "@" << load->GetDexPc();
    HInstruction* substitute = substitute_instructions_for_loads_[i];
    DCHECK(substitute != nullptr);
    // We proactively retrieve the substitute for a removed load, so
    // a load that has a substitute should not be observed as a heap
    // location value.
    DCHECK_EQ(FindSubstitute(substitute), substitute);

    // The load expects to load the heap value as type load->GetType().
    // However the tracked heap value may not be of that type. An explicit
    // type conversion may be needed.
    // There are actually three types involved here:
    // (1) tracked heap value's type (type A)
    // (2) heap location (field or element)'s type (type B)
    // (3) load's type (type C)
    // We guarantee that type A stored as type B and then fetched out as
    // type C is the same as casting from type A to type C directly, since
    // type B and type C will have the same size which is guaranteed in
    // HInstanceFieldGet/HStaticFieldGet/HArrayGet/HVecLoad's SetType().
    // So we only need one type conversion from type A to type C.
    HTypeConversion* type_conversion = AddTypeConversionIfNecessary(
        load, substitute, load->GetType());
    if (type_conversion != nullptr) {
      TryToReuseTypeConversion(type_conversion, i);
      load->ReplaceWith(type_conversion);
      substitute_instructions_for_loads_[i] = type_conversion;
    } else {
      load->ReplaceWith(substitute);
    }
    load->GetBlock()->RemoveInstruction(load);
  }

  // Finish marking stores for keeping and remove all the stores we can.
  SearchPhiPlaceholdersForKeptStores();
  for (const LoadStoreRecord& record : loads_and_stores_) {
    DCHECK_EQ(record.load_or_store->GetSideEffects().DoesAnyWrite(),
              record.stored_value != nullptr);
    if (record.stored_value != nullptr) {
      DCHECK(IsStore(record.load_or_store));
      if (!kept_stores_.IsBitSet(record.load_or_store->GetId())) {
        // TODO: Check if the written value is not identical with the one
        // present in the location after processing loop Phis.
        DCHECK(record.load_or_store->GetUses().empty()) << GetGraph()->PrettyMethod()
            << " " << record.load_or_store->DebugName() << "@" << record.load_or_store->GetId()
            << " used by " << record.load_or_store->GetUses().front().GetUser()->DebugName();
        record.load_or_store->GetBlock()->RemoveInstruction(record.load_or_store);
      }
    }
  }

  // Eliminate singleton-classified instructions:
  //   * - Constructor fences (they never escape this thread).
  //   * - Allocations (if they are unused).
  for (HInstruction* new_instance : singleton_new_instances_) {
    size_t removed = HConstructorFence::RemoveConstructorFences(new_instance);
    MaybeRecordStat(stats_,
                    MethodCompilationStat::kConstructorFenceRemovedLSE,
                    removed);

    if (!new_instance->HasNonEnvironmentUses()) {
      new_instance->RemoveEnvironmentUsers();
      new_instance->GetBlock()->RemoveInstruction(new_instance);
    }
  }
}

bool LoadStoreElimination::Run() {
  if (graph_->IsDebuggable() || graph_->HasTryCatch()) {
    // Debugger may set heap values or trigger deoptimization of callers.
    // Try/catch support not implemented yet.
    // Skip this optimization.
    return false;
  }
  ScopedArenaAllocator allocator(graph_->GetArenaStack());
  LoadStoreAnalysis lsa(graph_, &allocator);
  lsa.Run();
  const HeapLocationCollector& heap_location_collector = lsa.GetHeapLocationCollector();
  if (heap_location_collector.GetNumberOfHeapLocations() == 0) {
    // No HeapLocation information from LSA, skip this optimization.
    return false;
  }

  LSEVisitor lse_visitor(graph_, heap_location_collector, stats_);
  for (HBasicBlock* block : graph_->GetReversePostOrder()) {
    lse_visitor.VisitBasicBlock(block);
  }
  lse_visitor.RemoveInstructions();

  return true;
}

}  // namespace art
