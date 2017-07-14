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

public class ArrayLengthChanger extends CodeMutator {
  /**
   * Every CodeMutator has an AssociatedMutation, representing the
   * mutation that this CodeMutator can perform, to allow separate
   * generateMutation() and applyMutation() phases, allowing serialization.
   */
  public static class AssociatedMutation extends Mutation {
    public int ArraytochangeIdx;

    @Override
    public String getString() {
      return Integer.toString(ArraytochangeIdx);
    }

    @Override
    public void parseString(String[] elements) {
      ArraytochangeIdx = Integer.parseInt(elements[2]);
    }
  }

 //The following two methods are here for the benefit of MutationSerializer,
 // so it can create a CodeMutator and get the correct associated Mutation, as it
 // reads in mutations from a dump of mutations.
 @Override
 public Mutation getNewMutation() {
   return new AssociatedMutation();
 }

 public ArrayLengthChanger() { }

 public ArrayLengthChanger(Random rng, MutationStats stats, List<Mutation> mutations) {
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
     if (mInsn.insn.info.opcode == Opcode.ARRAY_LENGTH) {
       arrayLengthInsns.add(mInsn);
     }
   }
 }

 @Override
 protected boolean canMutate(MutatableCode mutatableCode) {
   for (MInsn mInsn : mutatableCode.getInstructions()) {
     if (mInsn.insn.info.opcode == Opcode.ARRAY_LENGTH) {
       return true;
     }
   }
   Log.debug("No Array length instruction in method, skipping...");
   return false;
 }

  @Override
  protected Mutation generateMutation(MutatableCode mutatableCode) {
    generateCachedArrayLengthInsns(mutatableCode);

    int arrayLengthIdx = rng.nextInt(arrayLengthInsns.size());

    AssociatedMutation mutation = new AssociatedMutation();
    mutation.setup(this.getClass(), mutatableCode);
    mutation.ArraytochangeIdx = arrayLengthIdx;
    return mutation;
  }

  @Override
  protected void applyMutation(Mutation uncastMutation) {
  // Cast the Mutation to our AssociatedMutation, so we can access its fields.
    AssociatedMutation mutation = (AssociatedMutation) uncastMutation;
    MutatableCode mutatableCode = mutation.mutatableCode;
    int arrayLengthIdx = mutation.ArraytochangeIdx;
    boolean foundInsnIdx = FoundInstruction(arrayLengthIdx);

    if (foundInsnIdx == false) {
      int newInsnIdx = arrayLengthIdx;
      MInsn arrayInsn = arrayLengthInsns.get(arrayLengthIdx);
      MInsn newInsn = new MInsn();
      newInsn.insn = new Instruction();
      newInsn.insn.info = Instruction.getOpcodeInfo(Opcode.CONST_16);
      newInsn.insn.vregA = (int) arrayInsn.insn.vregB;
      newInsn.insn.vregB = rng.nextInt(100);
      mutatableCode.insertInstructionAt(newInsn, newInsnIdx);
      Log.info("Changed the length of the array to " + newInsn.insn.vregB);
    }
    stats.incrementStat("Changed length of array");
    arrayLengthInsns = null;
  }

  protected boolean FoundInstruction (int arrayLengthIdx){
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