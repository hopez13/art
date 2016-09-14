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

#include "register_allocation_resolver.h"

#include "code_generator.h"
#include "ssa_liveness_analysis.h"

namespace art {

RegisterAllocationResolver::RegisterAllocationResolver(ArenaAllocator* allocator,
                                                       CodeGenerator* codegen,
                                                       const SsaLivenessAnalysis& liveness)
      : allocator_(allocator),
        codegen_(codegen),
        liveness_(liveness),
        coloring_(codegen_->GetGraph()->GetArena(), codegen_->GetGraph()->GetBlocks().size(),
                                             true, kArenaAllocLoopInfoExitEdges) {}

void RegisterAllocationResolver::Resolve(ArrayRef<HInstruction* const> safepoints,
                                         size_t reserved_out_slots,
                                         size_t int_spill_slots,
                                         size_t long_spill_slots,
                                         size_t float_spill_slots,
                                         size_t double_spill_slots,
                                         size_t catch_phi_spill_slots,
                                         const ArenaVector<LiveInterval*>& temp_intervals) {
  size_t spill_slots = int_spill_slots
                     + long_spill_slots
                     + float_spill_slots
                     + double_spill_slots
                     + catch_phi_spill_slots;

  // Update safepoints and calculate the size of the spills.
  UpdateSafepointLiveRegisters();
  size_t maximum_safepoint_spill_size = CalculateMaximumSafepointSpillSize(safepoints);

  // Computes frame size and spill mask.
  codegen_->InitializeCodeGeneration(spill_slots,
                                     maximum_safepoint_spill_size,
                                     reserved_out_slots,  // Includes slot(s) for the art method.
                                     codegen_->GetGraph()->GetLinearOrder());

  // Resolve outputs, including stack locations.
  // TODO: Use pointers of Location inside LiveInterval to avoid doing another iteration.
  for (size_t i = 0, e = liveness_.GetNumberOfSsaValues(); i < e; ++i) {
    HInstruction* instruction = liveness_.GetInstructionFromSsaIndex(i);
    LiveInterval* current = instruction->GetLiveInterval();
    LocationSummary* locations = instruction->GetLocations();
    Location location = locations->Out();
    if (instruction->IsParameterValue()) {
      // Now that we know the frame size, adjust the parameter's location.
      if (location.IsStackSlot()) {
        location = Location::StackSlot(location.GetStackIndex() + codegen_->GetFrameSize());
        current->SetSpillSlot(location.GetStackIndex());
        locations->UpdateOut(location);
      } else if (location.IsDoubleStackSlot()) {
        location = Location::DoubleStackSlot(location.GetStackIndex() + codegen_->GetFrameSize());
        current->SetSpillSlot(location.GetStackIndex());
        locations->UpdateOut(location);
      } else if (current->HasSpillSlot()) {
        current->SetSpillSlot(current->GetSpillSlot() + codegen_->GetFrameSize());
      }
    } else if (instruction->IsCurrentMethod()) {
      // The current method is always at offset 0.
      DCHECK(!current->HasSpillSlot() || (current->GetSpillSlot() == 0));
    } else if (instruction->IsPhi() && instruction->AsPhi()->IsCatchPhi()) {
      DCHECK(current->HasSpillSlot());
      size_t slot = current->GetSpillSlot()
                    + spill_slots
                    + reserved_out_slots
                    - catch_phi_spill_slots;
      current->SetSpillSlot(slot * kVRegSize);
    } else if (current->HasSpillSlot()) {
      // Adjust the stack slot, now that we know the number of them for each type.
      // The way this implementation lays out the stack is the following:
      // [parameter slots       ]
      // [catch phi spill slots ]
      // [double spill slots    ]
      // [long spill slots      ]
      // [float spill slots     ]
      // [int/ref values        ]
      // [maximum out values    ] (number of arguments for calls)
      // [art method            ].
      size_t slot = current->GetSpillSlot();
      switch (current->GetType()) {
        case Primitive::kPrimDouble:
          slot += long_spill_slots;
          FALLTHROUGH_INTENDED;
        case Primitive::kPrimLong:
          slot += float_spill_slots;
          FALLTHROUGH_INTENDED;
        case Primitive::kPrimFloat:
          slot += int_spill_slots;
          FALLTHROUGH_INTENDED;
        case Primitive::kPrimNot:
        case Primitive::kPrimInt:
        case Primitive::kPrimChar:
        case Primitive::kPrimByte:
        case Primitive::kPrimBoolean:
        case Primitive::kPrimShort:
          slot += reserved_out_slots;
          break;
        case Primitive::kPrimVoid:
          LOG(FATAL) << "Unexpected type for interval " << current->GetType();
      }
      current->SetSpillSlot(slot * kVRegSize);
    }

    Location source = current->ToLocation();

    if (location.IsUnallocated()) {
      if (location.GetPolicy() == Location::kSameAsFirstInput) {
        if (locations->InAt(0).IsUnallocated()) {
          locations->SetInAt(0, source);
        } else {
          DCHECK(locations->InAt(0).Equals(source));
        }
      }
      locations->UpdateOut(source);
    } else {
      DCHECK(source.Equals(location));
    }
  }

  // Connect siblings and resolve inputs.
  for (size_t i = 0, e = liveness_.GetNumberOfSsaValues(); i < e; ++i) {
    HInstruction* instruction = liveness_.GetInstructionFromSsaIndex(i);
    ConnectSiblings(instruction->GetLiveInterval());
  }

  // Resolve non-linear control flow across branches. Order does not matter.
  for (HLinearOrderIterator it(*codegen_->GetGraph()); !it.Done(); it.Advance()) {
    HBasicBlock* block = it.Current();
    if (block->IsCatchBlock() ||
        (block->IsLoopHeader() && block->GetLoopInformation()->IsIrreducible())) {
      // Instructions live at the top of catch blocks or irreducible loop header
      // were forced to spill.
      if (kIsDebugBuild) {
        BitVector* live = liveness_.GetLiveInSet(*block);
        for (uint32_t idx : live->Indexes()) {
          LiveInterval* interval = liveness_.GetInstructionFromSsaIndex(idx)->GetLiveInterval();
          LiveInterval* sibling = interval->GetSiblingAt(block->GetLifetimeStart());
          // `GetSiblingAt` returns the sibling that contains a position, but there could be
          // a lifetime hole in it. `CoversSlow` returns whether the interval is live at that
          // position.
          if ((sibling != nullptr) && sibling->CoversSlow(block->GetLifetimeStart())) {
            DCHECK(!sibling->HasRegister());
          }
        }
      }
    } else {
      BitVector* live = liveness_.GetLiveInSet(*block);
      for (uint32_t idx : live->Indexes()) {
        LiveInterval* interval = liveness_.GetInstructionFromSsaIndex(idx)->GetLiveInterval();
        for (HBasicBlock* predecessor : block->GetPredecessors()) {
          ConnectSplitSiblings(interval, predecessor, block);
        }
      }
    }
  }

  // Resolve phi inputs. Order does not matter.
  for (HLinearOrderIterator it(*codegen_->GetGraph()); !it.Done(); it.Advance()) {
    HBasicBlock* current = it.Current();
    if (current->IsCatchBlock()) {
      // Catch phi values are set at runtime by the exception delivery mechanism.
    } else {
      for (HInstructionIterator inst_it(current->GetPhis()); !inst_it.Done(); inst_it.Advance()) {
        HInstruction* phi = inst_it.Current();
        for (size_t i = 0, e = current->GetPredecessors().size(); i < e; ++i) {
          HBasicBlock* predecessor = current->GetPredecessors()[i];
          DCHECK_EQ(predecessor->GetNormalSuccessors().size(), 1u);
          HInstruction* input = phi->InputAt(i);
          Location source = input->GetLiveInterval()->GetLocationAt(
              predecessor->GetLifetimeEnd() - 1);
          Location destination = phi->GetLiveInterval()->ToLocation();
          InsertParallelMoveAtExitOf(predecessor, phi, source, destination);
        }
      }
    }
  }

  // Resolve temp locations.
  for (LiveInterval* temp : temp_intervals) {
    if (temp->IsHighInterval()) {
      // High intervals can be skipped, they are already handled by the low interval.
      continue;
    }
    HInstruction* at = liveness_.GetTempUser(temp);
    size_t temp_index = liveness_.GetTempIndex(temp);
    LocationSummary* locations = at->GetLocations();
    switch (temp->GetType()) {
      case Primitive::kPrimInt:
        locations->SetTempAt(temp_index, Location::RegisterLocation(temp->GetRegister()));
        break;

      case Primitive::kPrimDouble:
        if (codegen_->NeedsTwoRegisters(Primitive::kPrimDouble)) {
          Location location = Location::FpuRegisterPairLocation(
              temp->GetRegister(), temp->GetHighInterval()->GetRegister());
          locations->SetTempAt(temp_index, location);
        } else {
          locations->SetTempAt(temp_index, Location::FpuRegisterLocation(temp->GetRegister()));
        }
        break;

      default:
        LOG(FATAL) << "Unexpected type for temporary location "
                   << temp->GetType();
    }
  }
}

void RegisterAllocationResolver::UpdateSafepointLiveRegisters() {
  for (size_t i = 0, e = liveness_.GetNumberOfSsaValues(); i < e; ++i) {
    HInstruction* instruction = liveness_.GetInstructionFromSsaIndex(i);
    for (LiveInterval* current = instruction->GetLiveInterval();
         current != nullptr;
         current = current->GetNextSibling()) {
      if (!current->HasRegister()) {
        continue;
      }
      Location source = current->ToLocation();
      for (SafepointPosition* safepoint_position = current->GetFirstSafepoint();
           safepoint_position != nullptr;
           safepoint_position = safepoint_position->GetNext()) {
        DCHECK(current->CoversSlow(safepoint_position->GetPosition()));
        LocationSummary* locations = safepoint_position->GetLocations();
        switch (source.GetKind()) {
          case Location::kRegister:
          case Location::kFpuRegister: {
            locations->AddLiveRegister(source);
            break;
          }
          case Location::kRegisterPair:
          case Location::kFpuRegisterPair: {
            locations->AddLiveRegister(source.ToLow());
            locations->AddLiveRegister(source.ToHigh());
            break;
          }
          case Location::kStackSlot:  // Fall-through
          case Location::kDoubleStackSlot:  // Fall-through
          case Location::kConstant: {
            // Nothing to do.
            break;
          }
          default: {
            LOG(FATAL) << "Unexpected location for object";
          }
        }
      }
    }
  }
}

size_t RegisterAllocationResolver::CalculateMaximumSafepointSpillSize(
    ArrayRef<HInstruction* const> safepoints) {
  size_t core_register_spill_size = codegen_->GetWordSize();
  size_t fp_register_spill_size = codegen_->GetFloatingPointSpillSlotSize();
  size_t maximum_safepoint_spill_size = 0u;
  for (HInstruction* instruction : safepoints) {
    LocationSummary* locations = instruction->GetLocations();
    if (locations->OnlyCallsOnSlowPath()) {
      size_t core_spills =
          codegen_->GetNumberOfSlowPathSpills(locations, /* core_registers */ true);
      size_t fp_spills =
          codegen_->GetNumberOfSlowPathSpills(locations, /* core_registers */ false);
      size_t spill_size =
          core_register_spill_size * core_spills + fp_register_spill_size * fp_spills;
      maximum_safepoint_spill_size = std::max(maximum_safepoint_spill_size, spill_size);
    } else if (locations->CallsOnMainAndSlowPath()) {
      // Nothing to spill on the slow path if the main path already clobbers caller-saves.
      DCHECK_EQ(0u, codegen_->GetNumberOfSlowPathSpills(locations, /* core_registers */ true));
      DCHECK_EQ(0u, codegen_->GetNumberOfSlowPathSpills(locations, /* core_registers */ false));
    }
  }
  return maximum_safepoint_spill_size;
}

void RegisterAllocationResolver::ConnectSiblings(LiveInterval* interval) {
  LiveInterval* current = interval;
  if (current->HasSpillSlot()
      && current->HasRegister()
      // Currently, we spill unconditionnally the current method in the code generators.
      && !interval->GetDefinedBy()->IsCurrentMethod()) {
    // Catch blocks make spill placement exceptionally complicated.
    // Use the simple spilling algorithm in such case.
    if (codegen_->GetGraph()->HasTryCatch()) {
      // We spill eagerly, so move must be at definition.
      InsertMoveAfter(interval->GetDefinedBy(),
                    interval->ToLocation(),
                    interval->NeedsTwoSpillSlots()
                        ? Location::DoubleStackSlot(interval->GetParent()->GetSpillSlot())
                        : Location::StackSlot(interval->GetParent()->GetSpillSlot()));
    } else {
      // If the definition is in the loop-related blocks, it may cause excessive spill repetition.
      // Try to find more appropriate place for a move.
      PlaceSpills(interval);
    }
  }
  UsePosition* use = current->GetFirstUse();
  UsePosition* env_use = current->GetFirstEnvironmentUse();

  // Walk over all siblings, updating locations of use positions, and
  // connecting them when they are adjacent.
  do {
    Location source = current->ToLocation();

    // Walk over all uses covered by this interval, and update the location
    // information.

    LiveRange* range = current->GetFirstRange();
    while (range != nullptr) {
      while (use != nullptr && use->GetPosition() < range->GetStart()) {
        DCHECK(use->IsSynthesized());
        use = use->GetNext();
      }
      while (use != nullptr && use->GetPosition() <= range->GetEnd()) {
        DCHECK(!use->GetIsEnvironment());
        DCHECK(current->CoversSlow(use->GetPosition()) || (use->GetPosition() == range->GetEnd()));
        if (!use->IsSynthesized()) {
          LocationSummary* locations = use->GetUser()->GetLocations();
          Location expected_location = locations->InAt(use->GetInputIndex());
          // The expected (actual) location may be invalid in case the input is unused. Currently
          // this only happens for intrinsics.
          if (expected_location.IsValid()) {
            if (expected_location.IsUnallocated()) {
              locations->SetInAt(use->GetInputIndex(), source);
            } else if (!expected_location.IsConstant()) {
              AddInputMoveFor(interval->GetDefinedBy(), use->GetUser(), source, expected_location);
            }
          } else {
            DCHECK(use->GetUser()->IsInvoke());
            DCHECK(use->GetUser()->AsInvoke()->GetIntrinsic() != Intrinsics::kNone);
          }
        }
        use = use->GetNext();
      }

      // Walk over the environment uses, and update their locations.
      while (env_use != nullptr && env_use->GetPosition() < range->GetStart()) {
        env_use = env_use->GetNext();
      }

      while (env_use != nullptr && env_use->GetPosition() <= range->GetEnd()) {
        DCHECK(current->CoversSlow(env_use->GetPosition())
               || (env_use->GetPosition() == range->GetEnd()));
        HEnvironment* environment = env_use->GetEnvironment();
        environment->SetLocationAt(env_use->GetInputIndex(), source);
        env_use = env_use->GetNext();
      }

      range = range->GetNext();
    }

    // If the next interval starts just after this one, and has a register,
    // insert a move.
    LiveInterval* next_sibling = current->GetNextSibling();
    if (next_sibling != nullptr
        && next_sibling->HasRegister()
        && current->GetEnd() == next_sibling->GetStart()) {
      Location destination = next_sibling->ToLocation();
      InsertParallelMoveAt(current->GetEnd(), interval->GetDefinedBy(), source, destination);
    }

    for (SafepointPosition* safepoint_position = current->GetFirstSafepoint();
         safepoint_position != nullptr;
         safepoint_position = safepoint_position->GetNext()) {
      DCHECK(current->CoversSlow(safepoint_position->GetPosition()));

      if (current->GetType() == Primitive::kPrimNot) {
        DCHECK(interval->GetDefinedBy()->IsActualObject())
            << interval->GetDefinedBy()->DebugName()
            << "@" << safepoint_position->GetInstruction()->DebugName();
        LocationSummary* locations = safepoint_position->GetLocations();
        if (current->GetParent()->HasSpillSlot()) {
          locations->SetStackBit(current->GetParent()->GetSpillSlot() / kVRegSize);
        }
        if (source.GetKind() == Location::kRegister) {
          locations->SetRegisterBit(source.reg());
        }
      }
    }
    current = next_sibling;
  } while (current != nullptr);

  if (kIsDebugBuild) {
    // Following uses can only be synthesized uses.
    while (use != nullptr) {
      DCHECK(use->IsSynthesized());
      use = use->GetNext();
    }
  }
}

static bool IsMaterializableEntryBlockInstructionOfGraphWithIrreducibleLoop(
    HInstruction* instruction) {
  return instruction->GetBlock()->GetGraph()->HasIrreducibleLoops() &&
         (instruction->IsConstant() || instruction->IsCurrentMethod());
}

void RegisterAllocationResolver::ConnectSplitSiblings(LiveInterval* interval,
                                                      HBasicBlock* from,
                                                      HBasicBlock* to) const {
  if (interval->GetNextSibling() == nullptr) {
    // Nothing to connect. The whole range was allocated to the same location.
    return;
  }

  // Find the intervals that cover `from` and `to`.
  size_t destination_position = to->GetLifetimeStart();
  size_t source_position = from->GetLifetimeEnd() - 1;
  LiveInterval* destination = interval->GetSiblingAt(destination_position);
  LiveInterval* source = interval->GetSiblingAt(source_position);

  if (destination == source) {
    // Interval was not split.
    return;
  }

  LiveInterval* parent = interval->GetParent();
  HInstruction* defined_by = parent->GetDefinedBy();
  if (codegen_->GetGraph()->HasIrreducibleLoops() &&
      (destination == nullptr || !destination->CoversSlow(destination_position))) {
    // Our live_in fixed point calculation has found that the instruction is live
    // in the `to` block because it will eventually enter an irreducible loop. Our
    // live interval computation however does not compute a fixed point, and
    // therefore will not have a location for that instruction for `to`.
    // Because the instruction is a constant or the ArtMethod, we don't need to
    // do anything: it will be materialized in the irreducible loop.
    DCHECK(IsMaterializableEntryBlockInstructionOfGraphWithIrreducibleLoop(defined_by))
        << defined_by->DebugName() << ":" << defined_by->GetId()
        << " " << from->GetBlockId() << " -> " << to->GetBlockId();
    return;
  }

  if (!destination->HasRegister()) {
    // Values are eagerly spilled. Spill slot already contains appropriate value.
    return;
  }

  Location location_source;
  // `GetSiblingAt` returns the interval whose start and end cover `position`,
  // but does not check whether the interval is inactive at that position.
  // The only situation where the interval is inactive at that position is in the
  // presence of irreducible loops for constants and ArtMethod.
  if (codegen_->GetGraph()->HasIrreducibleLoops() &&
      (source == nullptr || !source->CoversSlow(source_position))) {
    DCHECK(IsMaterializableEntryBlockInstructionOfGraphWithIrreducibleLoop(defined_by));
    if (defined_by->IsConstant()) {
      location_source = defined_by->GetLocations()->Out();
    } else {
      DCHECK(defined_by->IsCurrentMethod());
      location_source = parent->NeedsTwoSpillSlots()
          ? Location::DoubleStackSlot(parent->GetSpillSlot())
          : Location::StackSlot(parent->GetSpillSlot());
    }
  } else {
    DCHECK(source != nullptr);
    DCHECK(source->CoversSlow(source_position));
    DCHECK(destination->CoversSlow(destination_position));
    location_source = source->ToLocation();
  }

  // If `from` has only one successor, we can put the moves at the exit of it. Otherwise
  // we need to put the moves at the entry of `to`.
  if (from->GetNormalSuccessors().size() == 1) {
    InsertParallelMoveAtExitOf(from,
                               defined_by,
                               location_source,
                               destination->ToLocation());
  } else {
    DCHECK_EQ(to->GetPredecessors().size(), 1u);
    InsertParallelMoveAtEntryOf(to,
                                defined_by,
                                location_source,
                                destination->ToLocation());
  }
}

static bool IsValidDestination(Location destination) {
  return destination.IsRegister()
      || destination.IsRegisterPair()
      || destination.IsFpuRegister()
      || destination.IsFpuRegisterPair()
      || destination.IsStackSlot()
      || destination.IsDoubleStackSlot();
}

void RegisterAllocationResolver::AddMove(HParallelMove* move,
                                         Location source,
                                         Location destination,
                                         HInstruction* instruction,
                                         Primitive::Type type) const {
  if (type == Primitive::kPrimLong
      && codegen_->ShouldSplitLongMoves()
      // The parallel move resolver knows how to deal with long constants.
      && !source.IsConstant()) {
    move->AddMove(source.ToLow(), destination.ToLow(), Primitive::kPrimInt, instruction);
    move->AddMove(source.ToHigh(), destination.ToHigh(), Primitive::kPrimInt, nullptr);
  } else {
    move->AddMove(source, destination, type, instruction);
  }
}

void RegisterAllocationResolver::AddInputMoveFor(HInstruction* input,
                                                 HInstruction* user,
                                                 Location source,
                                                 Location destination) const {
  if (source.Equals(destination)) return;

  DCHECK(!user->IsPhi());

  HInstruction* previous = user->GetPrevious();
  HParallelMove* move = nullptr;
  if (previous == nullptr
      || !previous->IsParallelMove()
      || previous->GetLifetimePosition() < user->GetLifetimePosition()) {
    move = new (allocator_) HParallelMove(allocator_);
    move->SetLifetimePosition(user->GetLifetimePosition());
    user->GetBlock()->InsertInstructionBefore(move, user);
  } else {
    move = previous->AsParallelMove();
  }
  DCHECK_EQ(move->GetLifetimePosition(), user->GetLifetimePosition());
  AddMove(move, source, destination, nullptr, input->GetType());
}

static bool IsInstructionStart(size_t position) {
  return (position & 1) == 0;
}

static bool IsInstructionEnd(size_t position) {
  return (position & 1) == 1;
}

void RegisterAllocationResolver::InsertParallelMoveAt(size_t position,
                                                      HInstruction* instruction,
                                                      Location source,
                                                      Location destination) const {
  DCHECK(IsValidDestination(destination)) << destination;
  if (source.Equals(destination)) return;

  HInstruction* at = liveness_.GetInstructionFromPosition(position / 2);
  HParallelMove* move;
  if (at == nullptr) {
    if (IsInstructionStart(position)) {
      // Block boundary, don't do anything the connection of split siblings will handle it.
      return;
    } else {
      // Move must happen before the first instruction of the block.
      at = liveness_.GetInstructionFromPosition((position + 1) / 2);
      // Note that parallel moves may have already been inserted, so we explicitly
      // ask for the first instruction of the block: `GetInstructionFromPosition` does
      // not contain the `HParallelMove` instructions.
      at = at->GetBlock()->GetFirstInstruction();

      if (at->GetLifetimePosition() < position) {
        // We may insert moves for split siblings and phi spills at the beginning of the block.
        // Since this is a different lifetime position, we need to go to the next instruction.
        DCHECK(at->IsParallelMove());
        at = at->GetNext();
      }

      if (at->GetLifetimePosition() != position) {
        DCHECK_GT(at->GetLifetimePosition(), position);
        move = new (allocator_) HParallelMove(allocator_);
        move->SetLifetimePosition(position);
        at->GetBlock()->InsertInstructionBefore(move, at);
      } else {
        DCHECK(at->IsParallelMove());
        move = at->AsParallelMove();
      }
    }
  } else if (IsInstructionEnd(position)) {
    // Move must happen after the instruction.
    DCHECK(!at->IsControlFlow());
    move = at->GetNext()->AsParallelMove();
    // This is a parallel move for connecting siblings in a same block. We need to
    // differentiate it with moves for connecting blocks, and input moves.
    if (move == nullptr || move->GetLifetimePosition() > position) {
      move = new (allocator_) HParallelMove(allocator_);
      move->SetLifetimePosition(position);
      at->GetBlock()->InsertInstructionBefore(move, at->GetNext());
    }
  } else {
    // Move must happen before the instruction.
    HInstruction* previous = at->GetPrevious();
    if (previous == nullptr
        || !previous->IsParallelMove()
        || previous->GetLifetimePosition() != position) {
      // If the previous is a parallel move, then its position must be lower
      // than the given `position`: it was added just after the non-parallel
      // move instruction that precedes `instruction`.
      DCHECK(previous == nullptr
             || !previous->IsParallelMove()
             || previous->GetLifetimePosition() < position);
      move = new (allocator_) HParallelMove(allocator_);
      move->SetLifetimePosition(position);
      at->GetBlock()->InsertInstructionBefore(move, at);
    } else {
      move = previous->AsParallelMove();
    }
  }
  DCHECK_EQ(move->GetLifetimePosition(), position);
  AddMove(move, source, destination, instruction, instruction->GetType());
}

void RegisterAllocationResolver::InsertParallelMoveAtExitOf(HBasicBlock* block,
                                                            HInstruction* instruction,
                                                            Location source,
                                                            Location destination) const {
  DCHECK(IsValidDestination(destination)) << destination;
  if (source.Equals(destination)) return;

  DCHECK_EQ(block->GetNormalSuccessors().size(), 1u);
  HInstruction* last = block->GetLastInstruction();
  // We insert moves at exit for phi predecessors and connecting blocks.
  // A block ending with an if or a packed switch cannot branch to a block
  // with phis because we do not allow critical edges. It can also not connect
  // a split interval between two blocks: the move has to happen in the successor.
  DCHECK(!last->IsIf() && !last->IsPackedSwitch());
  HInstruction* previous = last->GetPrevious();
  HParallelMove* move;
  // This is a parallel move for connecting blocks. We need to differentiate
  // it with moves for connecting siblings in a same block, and output moves.
  size_t position = last->GetLifetimePosition();
  if (previous == nullptr || !previous->IsParallelMove()
      || previous->AsParallelMove()->GetLifetimePosition() != position) {
    move = new (allocator_) HParallelMove(allocator_);
    move->SetLifetimePosition(position);
    block->InsertInstructionBefore(move, last);
  } else {
    move = previous->AsParallelMove();
  }
  AddMove(move, source, destination, instruction, instruction->GetType());
}

void RegisterAllocationResolver::InsertParallelMoveAtEntryOf(HBasicBlock* block,
                                                             HInstruction* instruction,
                                                             Location source,
                                                             Location destination) const {
  DCHECK(IsValidDestination(destination)) << destination;
  if (source.Equals(destination)) return;

  HInstruction* first = block->GetFirstInstruction();
  HParallelMove* move = first->AsParallelMove();
  size_t position = block->GetLifetimeStart();

  // Skip all of the placed spills to ensure the spilled registers are valid.
  while (move != nullptr && move->IsExplicitSpill()) {
    first = first->GetNext();
    move = first->AsParallelMove();
  }
  // From now on "first" is the first instruction of the block which is not a spill parallel move.

  // This is a parallel move for connecting blocks. We need to differentiate
  // it with moves for connecting siblings in a same block and input moves.
  if (move == nullptr || move->GetLifetimePosition() != position) {
    move = new (allocator_) HParallelMove(allocator_);
    move->SetLifetimePosition(position);
    block->InsertInstructionBefore(move, first);
  }
  AddMove(move, source, destination, instruction, instruction->GetType());
}

void RegisterAllocationResolver::InsertSpillParallelMoveAtEntryOf(HBasicBlock* block,
                                                    HInstruction* instruction,
                                                    Location source,
                                                    Location destination) const {
  DCHECK(IsValidDestination(destination)) << destination;
  if (source.Equals(destination)) return;

  HInstruction* first = block->GetFirstInstruction();
  HParallelMove* move = first->AsParallelMove();
  size_t position = block->GetLifetimeStart();
  // If there was no explicit splits found at the beginning of the block, create a new one.
  if (move == nullptr || !move->IsExplicitSpill()) {
    move = new (allocator_) HParallelMove(allocator_);
    move->SetLifetimePosition(position);
    move->SetExplicitSpill(true);
    block->InsertInstructionBefore(move, first);
  }
  // If one was found - add the current move to it.
  AddMove(move, source, destination, instruction, instruction->GetType());
}

void RegisterAllocationResolver::InsertMoveAfter(HInstruction* instruction,
                                                 Location source,
                                                 Location destination) const {
  DCHECK(IsValidDestination(destination)) << destination;
  if (source.Equals(destination)) return;

  if (instruction->IsPhi()) {
    InsertParallelMoveAtEntryOf(instruction->GetBlock(), instruction, source, destination);
    return;
  }

  size_t position = instruction->GetLifetimePosition() + 1;
  HParallelMove* move = instruction->GetNext()->AsParallelMove();
  // This is a parallel move for moving the output of an instruction. We need
  // to differentiate with input moves, moves for connecting siblings in a
  // and moves for connecting blocks.
  if (move == nullptr || move->GetLifetimePosition() != position) {
    move = new (allocator_) HParallelMove(allocator_);
    move->SetLifetimePosition(position);
    instruction->GetBlock()->InsertInstructionBefore(move, instruction->GetNext());
  }
  AddMove(move, source, destination, instruction, instruction->GetType());
}

// Creates a list of the blocks, which are the formal exit nodes of the given loop.
void RegisterAllocationResolver::FindExitEdges(HLoopInformation* loop) {
    FindExitEdgesRecursive(loop, loop->GetHeader());
    // Prepare the coloring vector for the next use.
    coloring_.ClearAllBits();
}

// Inner recursive implementation of the FindExitEdges method.
void RegisterAllocationResolver::FindExitEdgesRecursive(HLoopInformation* loop,
                                                        HBasicBlock* block) {
  // DFS, use additional bitvector for coloring
  coloring_.SetBit(block->GetBlockId());

  for (HBasicBlock* successor : block->GetSuccessors()) {
    if (loop->IsBackEdge(*successor)) {
      // Final point of the recursion.
    } else if (!coloring_.IsBitSet(successor->GetBlockId())) {
      if (!loop->GetBlocks().IsBitSet(successor->GetBlockId())) {
        // Add non-colored out-of-the-loop successor to the desired list.
        loop->AddExitEdge(successor);
      } else {
        // Walk into a non-colored in-the-loop successor.
        FindExitEdgesRecursive(loop, successor);
      }
    } else {
      if (!loop->GetBlocks().IsBitSet(successor->GetBlockId())) {
        // If the successor was already colored but we can reach it from the current.
        // node, then we shall tell about it.
        loop->SetExitNodesSimple(false);
      }
    }
  }
}

// Places spills in the least repetitive code fragments to produce less memory traffic.
// Improves the case when the variable definition is in a loop (either header or body)
// and the variable lives in registers within that whole loop.
// In such case the spills are better to be done at the least nested loop possible.
void RegisterAllocationResolver::PlaceSpills(LiveInterval* interval) {
  if (!interval->HasRegister()) {
    // The instruction type assumes the result is already spilled.
    return;
  }

  LiveInterval* parent = interval->GetParent();
  HInstruction* instruction = parent->GetDefinedBy();
  HBasicBlock* parent_block = instruction->GetBlock();
  HLoopInformation* parent_loop_info = parent_block->GetLoopInformation();
  Location source_location = parent->ToLocation();
  Location dest_location = parent->NeedsTwoSpillSlots()
                              ? Location::DoubleStackSlot(parent->GetSpillSlot())
                              : Location::StackSlot(parent->GetSpillSlot());

  if (parent_loop_info == nullptr) {
    // The parent is not in any loop - spilling right after the parent instruction won't
    // create any performance overhead due to store instruction repetition.
    InsertMoveAfter(instruction, source_location, dest_location);
    return;
  }

  // Check if any of the spilled intervals is inside of the same loop as parent's innermost one.
  // The task is to find the most nested loop possible which contains one of the spill
  // intervals and the parent instruction. Spills at this level of nesting are repeated
  // the least amount of times during execution.
  LiveInterval* spill_interval = interval->GetNextSpilledSibling();
  HLoopInformation* best_loop_info = parent_loop_info;
  while (spill_interval != nullptr) {
    HBasicBlock* spill_block = liveness_.GetBlockFromPosition(spill_interval->GetStart() / 2);
    size_t spill_block_id = spill_block->GetBlockId();
    if (parent_loop_info->GetBlocks().IsBitSet(spill_block_id)) {
      // The start of the spilled interval belongs to the parent's loop => the whole interval does.
      // That means we must do at least one spill inside of the loop, so let it dominate every
      // other spill interval preemptively.
      InsertMoveAfter(instruction, source_location, dest_location);
      return;
    }

    // For each spill interval we're trying to find the most nested loop which both contains
    // the interval and the parent instruction. Then we check if there's more nested one
    // which was found earlier for an other spill interval.
    HBasicBlock* prev_loop_preheader = parent_loop_info->GetPreHeader();
    HLoopInformation* prev_loop_info = parent_loop_info;
    HLoopInformation* current_loop_info = prev_loop_preheader->GetLoopInformation();
    while (current_loop_info != nullptr) {
      if (current_loop_info->GetBlocks().IsBitSet(spill_block_id)) {
        // The loop, containing parent, appears to contain the spill interval too.
        // Check whether the previous one if more nested than the currently best one.
        if (best_loop_info->GetBlocks().IsBitSet(prev_loop_preheader->GetBlockId())){
          // If it is - pick it as the new currently best one.
          best_loop_info = prev_loop_info;
        }
        break;
      }
      prev_loop_info = current_loop_info;
      prev_loop_preheader = current_loop_info->GetPreHeader();
      current_loop_info = prev_loop_preheader->GetLoopInformation();
    }
    spill_interval = spill_interval->GetNextSpilledSibling();
  }

  // Now best_loop_info is the loop, which does not contain any of spill intervals,
  // and the first outer loop contains some spill intervals and a parent instruction
  // It means that the set of best_loop_info exits dominates every spill interval. (*)
  // So the best place to put a spill in in term of a loop nesting depth is blocks
  // which are not in any deeper loops, and which belongs to best_loop_info.
  // The further analysis and search for a dominator of multiple spill intervals
  // require either an excessive amount of memory (storing full dominator tree) or
  // an excessive computational complexity.
  // It's better to just use the (*) statement and place the spills at the exit nodes,
  // since it gives the same code perfomance with a small overhead of a few additional
  // spills inserted (if any) of parallel edges of the CFG.
  if (!best_loop_info->NumberOfExitEdges()) {
    // If the exit nodes haven't been found already for the chosen loop, find them.
    FindExitEdges(best_loop_info);
  }
  // If one one the loop's exit nodes is connected to multiple in-loop nodes,
  // skip the advanced placement - we cannot be sure which register shall we spill.
  if (!best_loop_info->IsExitNodesSimple()) {
    // Avoid the complex spill placement.
    InsertMoveAfter(instruction, source_location, dest_location);
    return;
  } else {
    for (HBasicBlock* exit : best_loop_info->GetExitEdges()) {
      // Find a predecessor which stores the register location of the variable.
      for (HBasicBlock* predecessor : exit->GetPredecessors()) {
        if (predecessor->GetLoopInformation() == best_loop_info) {
          // That is, the exit edge.
          LiveInterval* source_interval = interval->GetSiblingAt(predecessor->GetLifetimeEnd() - 1);
          if (source_interval != nullptr) {
            // If the interval is dead (or not born) right before the exit
            // - it means the def occured in a loop, and an "upper" exit
            // can not be reached by the def. We're not interested in such cases.
            if (source_interval->CoversSlow(predecessor->GetLifetimeEnd() - 1)) {
              // If there's no active live range at the position of the loop exit
              // - the variable is not going to be used ever after. Skip such cases.
              source_location = source_interval->ToLocation();
              // Spill the corresponding register right at the current exit of the loop.
              InsertSpillParallelMoveAtEntryOf(exit, instruction,
                                                  source_location, dest_location);
              // We can break out of the predecessor search since we know the exit is "simple"
              // and we've already visited the only in-loop predecessor.
              break;
            }
          }
        }
      }
    }
  }
}

}  // namespace art
