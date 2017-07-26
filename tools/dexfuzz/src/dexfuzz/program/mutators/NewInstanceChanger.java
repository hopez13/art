/*
 * Copyright (C) 2017 The Android Open Source Project
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

package dexfuzz.program.mutators;

import dexfuzz.Log;
import dexfuzz.MutationStats;
import dexfuzz.program.IdCreator;
import dexfuzz.program.MInsn;
import dexfuzz.program.MutatableCode;
import dexfuzz.program.Mutation;
import dexfuzz.rawdex.Instruction;
import dexfuzz.rawdex.Opcode;
import dexfuzz.rawdex.formats.ContainsPoolIndex;
import dexfuzz.rawdex.formats.ContainsPoolIndex.PoolIndexKind;

import java.util.ArrayList;
import java.util.List;
import java.util.Random;
import jdk.nashorn.internal.runtime.FindProperty;

/**
 * Mutator NewInstanceChanger changes the new instance type in a method to
 * any random type from the pool.
 */
public class NewInstanceChanger extends CodeMutator {

  /**
   * Every CodeMutator has an AssociatedMutation, representing the
   * mutation that this CodeMutator can perform, to allow separate
   * generateMutation() and applyMutation() phases, allowing serialization.
   */
  public static class AssociatedMutation extends Mutation {
    public int newInstanceToChangeIdx;
    public int newInstanceTypeIdx;
    public int foundInsnIdx;

    @Override
    public String getString() {
      StringBuilder builder = new StringBuilder();
      builder.append(newInstanceToChangeIdx).append(" ");
      builder.append(newInstanceTypeIdx);
      builder.append(foundInsnIdx);
      return builder.toString();
    }

    @Override
    public void parseString(String[] elements) {
      newInstanceToChangeIdx = Integer.parseInt(elements[2]);
      newInstanceTypeIdx = Integer.parseInt(elements[3]);
      foundInsnIdx = Integer.parseInt(elements[4]);
    }
  }

  // The following two methods are here for the benefit of MutationSerializer,
  // so it can create a CodeMutator and get the correct associated Mutation, as it
  // reads in mutations from a dump of mutations.
  @Override
  public Mutation getNewMutation() {
    return new AssociatedMutation();
  }

  public NewInstanceChanger() {}

  public NewInstanceChanger(Random rng, MutationStats stats, List<Mutation> mutations) {
    super(rng, stats, mutations);
    likelihood = 30;
  }

  // A cache that should only exist between generateMutation() and applyMutation(),
  // or be created at the start of applyMutation(), if we're reading in mutations from
  // a file.
  private List<MInsn> newInstanceCachedInsns = null;

  private void generateCachedNewInstanceInsns(MutatableCode mutatableCode) {
    if (newInstanceCachedInsns != null) {
      return;
    }

    newInstanceCachedInsns = new ArrayList<MInsn>();

    for (MInsn mInsn : mutatableCode.getInstructions()) {
      if (mInsn.insn.info.opcode == Opcode.NEW_INSTANCE) {
        newInstanceCachedInsns.add(mInsn);
      }
    }
  }

  @Override
  protected boolean canMutate(MutatableCode mutatableCode) {
    for (MInsn mInsn : mutatableCode.getInstructions()) {
      // Cannot change the pool index with only one type.
      if (mutatableCode.program.getTotalPoolIndicesByKind(PoolIndexKind.Type) < 2) {
        return false;
      }
      if (mInsn.insn.info.opcode == Opcode.NEW_INSTANCE) {
        return true;
      }
    }
    Log.debug("No New Instance in method, skipping...");
    return false;
  }

  @Override
  protected Mutation generateMutation(MutatableCode mutatableCode) {
    generateCachedNewInstanceInsns(mutatableCode);

    int newInstanceIdxInCache = rng.nextInt(newInstanceCachedInsns.size());
    int newInstanceTypeIdx =
        rng.nextInt(mutatableCode.program.getTotalPoolIndicesByKind(PoolIndexKind.Type));
    int foundNewInstanceInsnIdx =
        foundInsnIdx(mutatableCode, newInstanceCachedInsns.get(newInstanceIdxInCache));
    

    AssociatedMutation mutation = new AssociatedMutation();
    mutation.setup(this.getClass(), mutatableCode);
    mutation.newInstanceToChangeIdx = newInstanceIdxInCache;
    mutation.newInstanceTypeIdx = newInstanceTypeIdx;
    mutation.foundInsnIdx = foundNewInstanceInsnIdx;
    return mutation;
  }

  @Override
  protected void applyMutation(Mutation uncastMutation) {
    // Cast the Mutation to our AssociatedMutation, so we can access its fields.
    AssociatedMutation mutation = (AssociatedMutation) uncastMutation;
    MutatableCode mutatableCode = mutation.mutatableCode;

    generateCachedNewInstanceInsns(mutatableCode);

    MInsn newInstanceInsn = newInstanceCachedInsns.get(mutation.newInstanceToChangeIdx);

    if (mutation.foundInsnIdx == -1 ||
        mutation.foundInsnIdx == mutatableCode.getInstructionCount()) {
      return;        
    }

    MInsn invokeInsn = mutatableCode.getInstructionAt(mutation.foundInsnIdx + 1);

    long oldTypeIdx =
        ((ContainsPoolIndex)newInstanceInsn.insn.info.format).getPoolIndex(newInstanceInsn.insn);

    long oldMethodIdx =
        ((ContainsPoolIndex)invokeInsn.insn.info.format).getPoolIndex(invokeInsn.insn);

    ((ContainsPoolIndex)newInstanceInsn.insn.info.format).
    setPoolIndex(newInstanceInsn.insn, mutation.newInstanceTypeIdx);

    if (invokeInsn.insn.info.opcode == Opcode.INVOKE_DIRECT ) {
      String[] methodName = mutatableCode.program.
          constructString(mutation.newInstanceTypeIdx, (int)oldMethodIdx);
      int methodId = mutatableCode.program.getNewItemCreator().
          findOrCreateMethodId(methodName[0], methodName[1], methodName[2]);

      ((ContainsPoolIndex)invokeInsn.insn.info.format).
      setPoolIndex(invokeInsn.insn, mutation.newInstanceTypeIdx);
      invokeInsn.insn.vregB = methodId;
    }

    Log.info("Changed " + oldTypeIdx + " to " + mutation.newInstanceTypeIdx);

    stats.incrementStat("Changed new instance.");

    // Clear cache.
    newInstanceCachedInsns = null;
  }

  // Check if there is an invoke call instruction, and if found, return the index
  protected int foundInsnIdx(MutatableCode mutatableCode, MInsn newInstanceInsn) {
    int i = 0;
    for (MInsn mInsn : mutatableCode.getInstructions()) {
      if (mInsn == newInstanceInsn) {
        return i;
      }
      i++;
    }
    return -1;
  }
}
