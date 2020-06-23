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
    DCHECK(IsLoad(load));
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
        LOG(ERROR) << "VMARKO: Marking phi for processing " << SpecialMarkerIndex(instruction);
      }
      phi_placeholders_to_search_for_kept_stores_.SetBit(SpecialMarkerIndex(instruction));
    } else {
      DCHECK(IsStore(instruction));
      if (verbose_) {
        LOG(ERROR) << "VMARKO: Keeping store " << AsInstruction(instruction)->GetId();
      }
      kept_possibly_removed_stores_.SetBit(AsInstruction(instruction)->GetId());
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

  void PrepareLoopValues(HBasicBlock* block);
  void MergePredecessorValues(HBasicBlock* block);

  void MaterializeNonLoopPhis(InstructionOrMarker phi_placeholder, size_t idx, DataType::Type type);
  InstructionOrMarker FindLoopPhisToMaterialize(InstructionOrMarker phi_placeholder,
                                                /*inout*/ArenaBitVector* visited);
  void MaterializeLoopPhis(const ArenaBitVector& current_dependencies,
                           const ScopedArenaVector<size_t>& phi_placeholder_indexes,
                           DataType::Type type);
  InstructionOrMarker TryToMaterializeLoopPhis(InstructionOrMarker phi_placeholder,
                                               DataType::Type type);
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
    for (size_t i = 0u, size = heap_values.size(); i != size; ++i) {
      if (verbose_) {
        LOG(ERROR) << "VMARKO: loc " << i << "/" << size << ": "
            << "unknown: " << (heap_values[i].stored_by == GetUnknownHeapValue())
            << " special: " << IsSpecialMarker(heap_values[i].stored_by);
      }
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
        DCHECK(heap_values[i].stored_by == GetUnknownHeapValue());
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

  // Stores in this list are candidates for removal but they may be later marked
  // for keeping in the `kept_possibly_removed_stores_`.
  ScopedArenaVector<HInstruction*> possibly_removed_stores_;
  // To avoid linear lookup in `possibly_removed_stores_` for removal of stores
  // we need to keep, we mark them in a bit vector indexed by instruction ID.
  ArenaBitVector kept_possibly_removed_stores_;
  // When we need to keep all stores that feed a Phi placeholder, we just record the
  // index of that placeholder for processing after graph traversal.
  ArenaBitVector phi_placeholders_to_search_for_kept_stores_;

  // Loads that would require a loop Phi to replace are recorded for processing
  // later as we do not have enough information from back-edges to determine if
  // a suitable Phi can be found or created when we visit these loads.
  ScopedArenaHashMap<HInstruction*, ValueDescription> loads_requiring_loop_phi_;

  // Replacements for Phi placeholders requiring a loop Phi.
  ScopedArenaVector<HInstruction*> loop_phi_replacements_;

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
      possibly_removed_stores_(allocator_.Adapter(kArenaAllocLSE)),
      // We may add new instructions (default values, Phis) but we're not adding stores,
      // so we do not need the following BitVector to be expandable.
      kept_possibly_removed_stores_(&allocator_,
                                    /*start_bits=*/ graph->GetCurrentInstructionId(),
                                    /*expandable=*/ false,
                                    kArenaAllocLSE),
      phi_placeholders_to_search_for_kept_stores_(&allocator_,
                                                  num_special_markers_,
                                                  /*expandable=*/ false,
                                                  kArenaAllocLSE),
      loads_requiring_loop_phi_(allocator_.Adapter(kArenaAllocLSE)),
      loop_phi_replacements_(allocator_.Adapter(kArenaAllocLSE)),
      singleton_new_instances_(allocator_.Adapter(kArenaAllocLSE)),
      verbose_(graph->PrettyMethod() ==
               "xint Main.testLoop2(TestClass, int)") {
  // Clear bit vectors.
  phi_placeholders_to_search_for_kept_stores_.ClearAllBits();
  kept_possibly_removed_stores_.ClearAllBits();
  // Reserve space for special markers and add markers for unknown and default value.
  special_markers_.reserve(num_special_markers_);
  special_markers_.push_back(static_cast<uint32_t>(-1));  // Unknown value.
  special_markers_.push_back(static_cast<uint32_t>(-1));  // Default value.
}

void LSEVisitor::PrepareLoopValues(HBasicBlock* block) {
  if (verbose_) {
    LOG(ERROR) << "VMARKO: PrepareLoopValues #" << block->GetBlockId();
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
  size_t phi_placeholders_begin = phi_placeholders_begin_for_block_[block->GetBlockId()];
  heap_values.reserve(num_heap_locations);
  for (size_t i = 0u; i != num_heap_locations; ++i) {
    // If the pre-header value is known, use a Phi placeholder for the value in the loop
    // header. If all predecessors are later found to have a known value, we may replace
    // loads from this location, either with the pre-header value or with a new Phi.
    InstructionOrMarker phi_placeholder = &special_markers_[phi_placeholders_begin + i];
    bool needs_loop_phi = (pre_header_heap_values[i].value != GetUnknownHeapValue());
    InstructionOrMarker value = needs_loop_phi ? phi_placeholder : GetUnknownHeapValue();
    // Use the Phi placeholder for `stored_by` to make sure all incoming stores are kept
    // if the value in the location escapes. This is not applicable to locations that are
    // not live in the loop.
    ReferenceInfo* ref_info = heap_location_collector_.GetHeapLocation(i)->GetReferenceInfo();
    HBasicBlock* ref_block = ref_info->GetReference()->GetBlock();
    bool strictly_dominates = (ref_block != block) && ref_block->Dominates(block);
    InstructionOrMarker stored_by = strictly_dominates ? phi_placeholder : GetUnknownHeapValue();
    heap_values.push_back({ value, needs_loop_phi, stored_by });
  }
}

void LSEVisitor::MergePredecessorValues(HBasicBlock* block) {
  if (block->IsExitBlock()) {
    // Exit block doesn't really merge values since the control flow ends in
    // its predecessors. Each predecessor needs to make sure stores are kept
    // if necessary.
    return;
  }

  if (verbose_) {
    LOG(ERROR) << "VMARKO: MergedPredecessorValues #" << block->GetBlockId()
        << " <-" << [&]() {
          std::ostringstream oss;
          for (HBasicBlock* p : block->GetPredecessors()) {
            oss << " " << p->GetBlockId();
          }
          return oss.str();
        }();
  }
  ScopedArenaVector<ValueDescription>& heap_values = heap_values_for_[block->GetBlockId()];
  DCHECK(heap_values.empty());
  size_t num_heap_locations = heap_location_collector_.GetNumberOfHeapLocations();
  ArrayRef<HBasicBlock* const> predecessors(block->GetPredecessors());
  if (predecessors.size() == 0) {
    DCHECK(block->IsEntryBlock());
    heap_values.resize(num_heap_locations, GetUnknownHeapValueDescription());
    return;
  }

  size_t phi_placeholders_begin = phi_placeholders_begin_for_block_[block->GetBlockId()];
  heap_values.reserve(num_heap_locations);
  for (size_t i = 0u; i != num_heap_locations; ++i) {
    ReferenceInfo* ref_info = heap_location_collector_.GetHeapLocation(i)->GetReferenceInfo();
    HInstruction* ref = ref_info->GetReference();
    HInstruction* singleton_ref = nullptr;
    if (ref_info->IsSingleton()) {
      // We do more analysis based on singleton's liveness when merging
      // heap values for such cases.
      singleton_ref = ref;
    }

    ValueDescription merged_description = {
        /*value=*/ nullptr,
        /*needs_loop_phi=*/ false,
        /*stored_by=*/ nullptr
    };
    for (HBasicBlock* predecessor : predecessors) {
      DCHECK(!heap_values_for_[predecessor->GetBlockId()].empty());
      ValueDescription pred_description = heap_values_for_[predecessor->GetBlockId()][i];
      DCHECK(pred_description.value != nullptr);
      DCHECK(pred_description.stored_by != nullptr);
      if (merged_description.value == nullptr) {
        // First seen heap value description.
        merged_description = pred_description;
        continue;
      }
      if (merged_description.value != GetUnknownHeapValue()) {
        if (pred_description.value == GetUnknownHeapValue()) {
          merged_description.value = GetUnknownHeapValue();
          merged_description.needs_loop_phi = false;
        } else if (pred_description.value != merged_description.value) {
          // There are conflicting known values. We may still be able to replace loads with a Phi.
          InstructionOrMarker phi_placeholder = &special_markers_[phi_placeholders_begin + i];
          merged_description.value = phi_placeholder;
          // Propagate the need for a new loop Phi from all predecessors.
          merged_description.needs_loop_phi |= pred_description.needs_loop_phi;
        }
      }
      if (merged_description.stored_by != pred_description.stored_by) {
        // Use the marker Phi to track if we need to keep different stores.
        InstructionOrMarker phi_placeholder = &special_markers_[phi_placeholders_begin + i];
        merged_description.stored_by = phi_placeholder;
      }
    }

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
    DCHECK(!current_block->IsLoopHeader());
    phi_inputs.clear();
    for (HBasicBlock* predecessor : current_block->GetPredecessors()) {
      ValueDescription& pred_description = heap_values_for_[predecessor->GetBlockId()][idx];
      if (IsSpecialMarker(pred_description.value)) {
        DCHECK(pred_description.value != GetUnknownHeapValue());
        if (pred_description.value == GetDefaultHeapValue()) {
          phi_inputs.push_back(GetDefaultValue(type));
        } else {
          // We need to process the block where this phi placeholder originates.
          uint32_t phi_block_id = GetPhiPlaceholderBlockId(pred_description.value);
          DCHECK_EQ(idx, GetPhiPlaceholderLocationIndex(pred_description.value, phi_block_id));
          work_queue.push_back(phi_block_id);
        }
      } else {
        phi_inputs.push_back(AsInstruction(pred_description.value));
      }
    }
    if (phi_inputs.size() == current_block->GetPredecessors().size()) {
      // All inputs are available. Find or construct the Phi.
      HInstruction* replacement_phi = FindOrConstructNonLoopPhi(current_block, phi_inputs, type);
      size_t phi_placeholders_begin = phi_placeholders_begin_for_block_[current_block_id];
      InstructionOrMarker current_phi_placeholder = &special_markers_[phi_placeholders_begin + idx];
      // Replace the Phi placeholder in `heap_values_for_` for current block and all successors.
      // Reuse the work queue and remove the current block id in the process.
      DCHECK_EQ(current_block_id, work_queue.back());
      size_t end_size = work_queue.size() - 1u;
      while (work_queue.size() != end_size) {
        uint32_t replacement_block_id = work_queue.back();
        work_queue.pop_back();
        if (heap_values_for_[replacement_block_id][idx].value == current_phi_placeholder) {
          heap_values_for_[replacement_block_id][idx].value = replacement_phi;
          for (HBasicBlock* successor : blocks[replacement_block_id]->GetSuccessors()) {
            if (!heap_values_for_[successor->GetBlockId()].empty()) {
              work_queue.push_back(successor->GetBlockId());
            }
          }
        }
      }
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
    for (HBasicBlock* predecessor : current_block->GetPredecessors()) {
      const ValueDescription& description = heap_values_for_[predecessor->GetBlockId()][idx];
      if (description.value == GetUnknownHeapValue()) {
        // We cannot create a Phi for this loop Phi placeholder.
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
    LOG(ERROR) << "VMARKO: Materializing " << current_dependencies.NumSetBits();
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
      const ValueDescription& description = heap_values_for_[predecessor->GetBlockId()][idx];
      if (!description.needs_loop_phi &&
          IsSpecialMarker(description.value) &&
          description.value != GetDefaultHeapValue()) {
        MaterializeNonLoopPhis(description.value, idx, type);
        DCHECK(!IsSpecialMarker(description.value));
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
    HInstruction* phi = loop_phi_replacements_[phi_placeholder_index];
    for (size_t i = 0, size = block->GetPredecessors().size(); i != size; ++i) {
      HBasicBlock* predecessor = block->GetPredecessors()[i];
      const ValueDescription& description = heap_values_for_[predecessor->GetBlockId()][idx];
      HInstruction* input;
      if (description.needs_loop_phi) {
        input = loop_phi_replacements_[SpecialMarkerIndex(description.value)];
        DCHECK(input != nullptr);
      } else {
        input = (description.value == GetDefaultHeapValue()) ? GetDefaultValue(type)
                                                             : AsInstruction(description.value);
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
    block->AddPhi(loop_phi_replacements_[phi_placeholder_index]->AsPhi());
  }
  if (type == DataType::Type::kReference) {
    ScopedArenaAllocator local_allocator(allocator_.GetArenaStack());
    ScopedArenaVector<HInstruction*> phis(local_allocator.Adapter(kArenaAllocLSE));
    for (uint32_t matrix_index : current_dependencies.Indexes()) {
      size_t phi_placeholder_index = phi_placeholder_indexes[matrix_index];
      phis.push_back(loop_phi_replacements_[phi_placeholder_index]);
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
  if (loop_phi_replacements_[SpecialMarkerIndex(phi_placeholder)] != nullptr) {
    return nullptr;  // Already materialized. Report success.
  }

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

void LSEVisitor::ProcessLoadsRequiringLoopPhis() {
  // Pre-allocate replacements array before constructing nested `ScopedArenaAllocator`.
  loop_phi_replacements_.resize(num_special_markers_, nullptr);

  for (auto& entry : loads_requiring_loop_phi_) {
    InstructionOrMarker loop_phi_with_unknown_input =
        TryToMaterializeLoopPhis(entry.second.value, entry.first->GetType());
    if (loop_phi_with_unknown_input != nullptr) {
      DCHECK(loop_phi_replacements_[SpecialMarkerIndex(entry.second.value)] == nullptr);
      // TODO: Re-process loads and stores in successors from the `loop_phi_with_unknown_input`.
      // This should find at least one load from `loads_requiring_loop_phi_` which cannot be
      // replaced by Phis but not necessarily the one we just processed. Propagate the load(s)
      // as the new value(s) to successors. This may uncover new elimination opportunities.
      // (Even the current `entry` may eventually be replaced by Phis.)
    }
  }

  // Keep stores that feed loads that were not replaced.
  for (auto& entry : loads_requiring_loop_phi_) {
    ValueDescription& description = entry.second;
    DCHECK(IsPhiPlaceholder(description.value));
    if (loop_phi_replacements_[SpecialMarkerIndex(description.value)] != nullptr) {
      AddRemovedLoad(entry.first, loop_phi_replacements_[SpecialMarkerIndex(description.value)]);
    } else {
      KeepStores(description.stored_by);
    }
  }
}

void LSEVisitor::VisitGetLocation(HInstruction* instruction, size_t idx) {
  DCHECK_NE(idx, HeapLocationCollector::kHeapLocationNotFound);
  uint32_t block_id = instruction->GetBlock()->GetBlockId();
  ScopedArenaVector<ValueDescription>& heap_values = heap_values_for_[block_id];
  ValueDescription& description = heap_values[idx];
  if (verbose_) {
    LOG(ERROR) << "VMARKO: VisitGetLocation "
        << instruction->DebugName() << "@" << instruction->GetDexPc() << " " << idx;
  }
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
    DCHECK(IsSpecialMarker(description.value));
    DCHECK(IsSpecialMarker(description.stored_by)) << GetGraph()->PrettyMethod();
    loads_requiring_loop_phi_.insert(std::make_pair(instruction, description));
  } else {
    // This load can be eliminated but we may need to construct non-loop Phis.
    if (IsSpecialMarker(description.value)) {
      if (verbose_) {
        LOG(ERROR) << "VMARKO: Replacing IGET in block " << block_id
            << " predecessors: " << instruction->GetBlock()->GetPredecessors().size();
      }
      MaterializeNonLoopPhis(description.value, idx, instruction->GetType());
    }
    DCHECK(!IsSpecialMarker(description.value));
    HInstruction* heap_value = FindSubstitute(AsInstruction(description.value));
    DCHECK_NE(heap_value, GetUnknownHeapValue());
    AddRemovedLoad(instruction, AsInstruction(heap_value));
    TryRemovingNullCheck(instruction);
  }
}

void LSEVisitor::VisitSetLocation(HInstruction* instruction, size_t idx, HInstruction* value) {
  if (verbose_) {
    LOG(ERROR) << "VMARKO: VisitSetLocation "
        << instruction->DebugName() << "@" << instruction->GetDexPc() << " " << idx;
  }
  DCHECK_NE(idx, HeapLocationCollector::kHeapLocationNotFound);
  DCHECK(!IsStore(value)) << value->DebugName();
  // value may already have a substitute.
  value = FindSubstitute(value);
  ScopedArenaVector<ValueDescription>& heap_values =
      heap_values_for_[instruction->GetBlock()->GetBlockId()];
  ValueDescription& description = heap_values[idx];

  if (Equal(description.value, value)) {
    // Store into the heap location with the same value.
    // This store can be eliminated right away.
    instruction->GetBlock()->RemoveInstruction(instruction);
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
  if (instruction->CanThrow()) {
    // We cannot remove a possibly throwing store. Therefore we do not need to track it
    // in the value description's `stored_by`. However, previous stores can become visible.
    // TODO: Add a test for this.
    KeepStores(description.stored_by);
    description.stored_by = GetUnknownHeapValue();
  } else {
    if (verbose_) {
      LOG(ERROR) << "VMARKO: possibly removed store: "
          << instruction->DebugName() << "@" << instruction->GetDexPc() << " " << idx
          << " id: " << instruction->GetId();
    }
    possibly_removed_stores_.push_back(instruction);
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
  while (!work_queue.empty()) {
    InstructionOrMarker phi_placeholder = &special_markers_[work_queue.back()];
    work_queue.pop_back();
    uint32_t block_id = GetPhiPlaceholderBlockId(phi_placeholder);
    size_t idx = GetPhiPlaceholderLocationIndex(phi_placeholder, block_id);
    if (verbose_) {
      LOG(ERROR) << "VMARKO: Processing Phi placeholder for keeping "
          << block_id << " idx: " << idx;
    }
    for (HBasicBlock* predecessor : GetGraph()->GetBlocks()[block_id]->GetPredecessors()) {
      InstructionOrMarker stored_by = heap_values_for_[predecessor->GetBlockId()][idx].stored_by;
      if (IsSpecialMarker(stored_by)) {
        if (stored_by != GetUnknownHeapValue()) {
          DCHECK(IsPhiPlaceholder(stored_by));
          size_t phi_placeholder_index = SpecialMarkerIndex(stored_by);
          if (!phi_placeholders_to_search_for_kept_stores_.IsBitSet(phi_placeholder_index)) {
            phi_placeholders_to_search_for_kept_stores_.SetBit(phi_placeholder_index);
            work_queue.push_back(phi_placeholder_index);
          }
        }
      } else {
        DCHECK(IsStore(stored_by));
        if (verbose_) {
          LOG(ERROR) << "VMARKO: Keeping store for Phi " << AsInstruction(stored_by)->GetId();
        }
        kept_possibly_removed_stores_.SetBit(AsInstruction(stored_by)->GetId());
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
    // type B and type C will have the same size which is guarenteed in
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

  // At this point, stores marked for removal can be safely removed.
  SearchPhiPlaceholdersForKeptStores();
  for (HInstruction* store : possibly_removed_stores_) {
    DCHECK(IsStore(store));
    if (!kept_possibly_removed_stores_.IsBitSet(store->GetId())) {
      // TODO: Check if the written value is not identical with the one
      // present in the location after processing loop Phis.
      store->GetBlock()->RemoveInstruction(store);
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
#if 0
results should change with loop Phis for test21() when allowing reference Phis.

#endif

}  // namespace art
