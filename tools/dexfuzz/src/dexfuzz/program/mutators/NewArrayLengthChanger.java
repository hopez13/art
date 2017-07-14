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
import dexfuzz.program.MInsn;
import dexfuzz.program.MutatableCode;
import dexfuzz.program.Mutation;
import dexfuzz.rawdex.Instruction;
import dexfuzz.rawdex.Opcode;
import java.util.ArrayList;
import java.util.List;
import java.util.Random;

public class NewArrayLengthChanger extends CodeMutator {
  /**
   * Every CodeMutator has an AssociatedMutation, representing the
   * mutation that this CodeMutator can perform, to allow separate
   * generateMutation() and applyMutation() phases, allowing serialization.
   */
  public static class AssociatedMutation extends Mutation {
    public int arrayToChangeIdx;

    @Override
    public String getString() {
      return Integer.toString(arrayToChangeIdx);
    }

    @Override
    public void parseString(String[] elements) {
      arrayToChangeIdx = Integer.parseInt(elements[2]);
    }
  }

  //The following two methods are here for the benefit of MutationSerializer,
  // so it can create a CodeMutator and get the correct associated Mutation, as it
  // reads in mutations from a dump of mutations.
  @Override
  public Mutation getNewMutation() {
    return new AssociatedMutation();
  }

  public NewArrayLengthChanger() { }

  public NewArrayLengthChanger(Random rng, MutationStats stats, List<Mutation> mutations) {
    super(rng, stats, mutations);
    likelihood = 50;
  }

  // A cache that should only exist between generateMutation() and applyMutation(),
  // or be created at the start of applyMutation(), if we're reading in mutations from
  // a file.
  private List<MInsn> arrayLengthInsns = null;

  private void generateCachedArrayLengthInsns(MutatableCode mutatableCode) {
    if (arrayLengthInsns != null) {
      return;
    }

    arrayLengthInsns = new ArrayList<MInsn>();

    for (MInsn mInsn : mutatableCode.getInstructions()) {
      if (mInsn.insn.info.opcode == Opcode.NEW_ARRAY) {
        arrayLengthInsns.add(mInsn);
      }
    }
  }

  @Override
  protected boolean canMutate(MutatableCode mutatableCode) {
    for (MInsn mInsn : mutatableCode.getInstructions()) {
      Opcode opcode = mInsn.insn.info.opcode;
      //TODO: Add filled-new-array and filled-new-array/range with their respective
      //positions of registers and also proper encoding. 
      if (isNewArray(opcode)) {
        return true;
      }
    }
    Log.debug("No Array length instruction in method, skipping...");
    return false;
  }

  protected boolean isNewArray(Opcode opcode) {
    if ( opcode == Opcode.NEW_ARRAY) {
      return true;
    }
    return false;
  }

  @Override
  protected Mutation generateMutation(MutatableCode mutatableCode) {
    generateCachedArrayLengthInsns(mutatableCode);

    int arrayLengthIdx = rng.nextInt(arrayLengthInsns.size());

    AssociatedMutation mutation = new AssociatedMutation();
    mutation.setup(this.getClass(), mutatableCode);
    mutation.arrayToChangeIdx = arrayLengthIdx;
    return mutation;
  }

  @Override
  protected void applyMutation(Mutation uncastMutation) {
  // Cast the Mutation to our AssociatedMutation, so we can access its fields.
    AssociatedMutation mutation = (AssociatedMutation) uncastMutation;
    MutatableCode mutatableCode = mutation.mutatableCode;
    int arrayLengthIdx = mutation.arrayToChangeIdx;
    boolean foundInsnIdx = foundInstruction(arrayLengthIdx);

    if (foundInsnIdx == false) {
      int newInsnIdx = arrayLengthIdx;
      MInsn arrayInsn = arrayLengthInsns.get(arrayLengthIdx);
      MInsn newInsn = new MInsn();
      newInsn.insn = new Instruction();
      newInsn.insn.info = Instruction.getOpcodeInfo(Opcode.CONST_16);
      newInsn.insn.vregA = (int) arrayInsn.insn.vregB;
      //New length chosen randomly between 1 to 100.
      newInsn.insn.vregB = rng.nextInt(100);
      mutatableCode.insertInstructionAt(newInsn, newInsnIdx);
      Log.info("Changed the length of the array to " + newInsn.insn.vregB);
    }
    stats.incrementStat("Changed length of new array");
    arrayLengthInsns = null;
  }

  protected boolean foundInstruction(int arrayLengthIdx){
    MInsn arrayInsn = arrayLengthInsns.get(arrayLengthIdx);
    int regB = (int) arrayInsn.insn.vregB;
    for (int i = arrayLengthIdx - 1; i >= 0; i--) {
      if (isConst(i)) {
        MInsn arrayInstruction = arrayLengthInsns.get(i);
        int tempReg = (int) arrayInstruction.insn.vregA;
        if (tempReg == regB) {
          arrayInstruction.insn.vregA = rng.nextInt(100);
          Log.info("Changed the length of the array to " + arrayInstruction.insn.vregB);
          return true;
        }
      }
    }
    return false;
  }

  protected boolean isConst(int arrayIdx) {
    MInsn arrayInstruction = arrayLengthInsns.get(arrayIdx);
    Opcode opcode = arrayInstruction.insn.info.opcode;
    if (opcode == Opcode.CONST_4 || opcode == Opcode.CONST_16 || opcode == Opcode.CONST) {
      return true;
    }
    return false;
  }
}