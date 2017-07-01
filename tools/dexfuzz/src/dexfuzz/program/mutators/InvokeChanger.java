/*
 * Copyright (C) 2014 The Android Open Source Project
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
import dexfuzz.program.MInsn;
import dexfuzz.program.MutatableCode;
import dexfuzz.program.Mutation;
import dexfuzz.rawdex.Instruction;
import dexfuzz.rawdex.Opcode;

import java.util.ArrayList;
import java.util.List;
import java.util.Random;

public class InvokeChanger extends CodeMutator {
  /**
   * Every CodeMutator has an AssociatedMutation, representing the
   * mutation that this CodeMutator can perform, to allow separate
   * generateMutation() and applyMutation() phases, allowing serialization.
   */
  public static class AssociatedMutation extends Mutation {
    public int invokeCallInsnIdx;

    @Override
    public String getString() {
      return Integer.toString(invokeCallInsnIdx);
    }

    @Override
    public void parseString(String[] elements) {
      invokeCallInsnIdx = Integer.parseInt(elements[2]);
    }
  }

  // The following two methods are here for the benefit of MutationSerializer,
  // so it can create a CodeMutator and get the correct associated Mutation, as it
  // reads in mutations from a dump of mutations.
  @Override
  public Mutation getNewMutation() {
    return new AssociatedMutation();
  }

  public InvokeChanger() { }

  public InvokeChanger(Random rng, MutationStats stats, List<Mutation> mutations) {
    super(rng, stats, mutations);
    likelihood = 30;
  }

  // A cache that should only exist between generateMutation() and applyMutation(),
  // or be created at the start of applyMutation(), if we're reading in mutations from
  // a file.
  private List<MInsn> invokeCallInsns = null;

  private void generateCachedinvokeCallInsns(MutatableCode mutatableCode) {
    if (invokeCallInsns != null) {
      return;
    }

    invokeCallInsns = new ArrayList<MInsn>();

    for (MInsn mInsn : mutatableCode.getInstructions()) {
      if (isInvokeCallInst(mInsn)) {
        invokeCallInsns.add(mInsn);
      }
    }
  }

  @Override
  protected boolean canMutate(MutatableCode mutatableCode) {
    for (MInsn mInsn : mutatableCode.getInstructions()) {
      if (isInvokeCallInst(mInsn)) {
        return true;
      }
    }

    Log.debug("No invoke instruction in method, skipping...");
    return false;
  }

  @Override
  protected Mutation generateMutation(MutatableCode mutatableCode) {
    generateCachedinvokeCallInsns(mutatableCode);

    int invokeCallInsnIdx = rng.nextInt(invokeCallInsns.size());

    AssociatedMutation mutation = new AssociatedMutation();
    mutation.setup(this.getClass(), mutatableCode);
    mutation.invokeCallInsnIdx = invokeCallInsnIdx;
    return mutation;
  }

  @Override
  protected void applyMutation(Mutation uncastMutation) {
    // Cast the Mutation to our AssociatedMutation, so we can access its fields.
    AssociatedMutation mutation = (AssociatedMutation) uncastMutation;
    MutatableCode mutatableCode = mutation.mutatableCode;

    generateCachedinvokeCallInsns(mutatableCode);

    MInsn invokeInsn = invokeCallInsns.get(mutation.invokeCallInsnIdx);

    String oldInsnString = invokeInsn.toString();

    Opcode newOpcode = getLegalDifferentOpcode(invokeInsn);

    invokeInsn.insn.info = Instruction.getOpcodeInfo(newOpcode);

    Log.info("Changed " + oldInsnString + " to " + invokeInsn);

    stats.incrementStat("Changed invoke call instruction");

    // Clear cache.
    invokeCallInsns = null;
  }

  private Opcode getLegalDifferentOpcode(MInsn mInsn) {
    Opcode opcode = mInsn.insn.info.opcode;
    final Opcode[] invokeList = {
        Opcode.INVOKE_VIRTUAL,
        Opcode.INVOKE_SUPER,
        Opcode.INVOKE_DIRECT,
        Opcode.INVOKE_STATIC,
        Opcode.INVOKE_INTERFACE,
      };

      final Opcode[] invokeRangeList = {
        Opcode.INVOKE_VIRTUAL_RANGE,
        Opcode.INVOKE_SUPER_RANGE,
        Opcode.INVOKE_DIRECT_RANGE,
        Opcode.INVOKE_STATIC_RANGE,
        Opcode.INVOKE_INTERFACE_RANGE,
      };

      if (Opcode.isBetween(opcode, Opcode.INVOKE_VIRTUAL, Opcode.INVOKE_INTERFACE)) {
        int index = opcode.ordinal() - Opcode.INVOKE_VIRTUAL.ordinal();
        int length = invokeList.length;
        return invokeList[(index + 1 + rng.nextInt(length - 1)) % length];
      } else if (Opcode.isBetween(opcode, Opcode.INVOKE_VIRTUAL_RANGE, Opcode.INVOKE_INTERFACE_RANGE)) {
        int index = opcode.ordinal() - Opcode.INVOKE_VIRTUAL_RANGE.ordinal();
        int length = invokeRangeList.length;
        return invokeRangeList[(index + 1 + rng.nextInt(length - 1)) % length];
        }
        return opcode;
      }

  private boolean isInvokeCallInst(MInsn mInsn) {
    Opcode opcode = mInsn.insn.info.opcode;
    if (Opcode.isBetween(opcode, Opcode.INVOKE_VIRTUAL, Opcode.INVOKE_INTERFACE_RANGE)) {
      if (opcode != Opcode.RETURN_VOID_NO_BARRIER){
        return true;
      }
      return false;
    }
    return false;
  }
}
