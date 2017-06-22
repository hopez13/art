package dexfuzz.program.mutators;

import dexfuzz.MutationStats;
import dexfuzz.program.MInsn;
import dexfuzz.program.Mutation;
import dexfuzz.rawdex.Opcode;
import java.util.Arrays;
import java.util.List;
import java.util.Random;

public class RandomBranchChanger extends IfBranchChanger {

  public RandomBranchChanger(Random rng, MutationStats stats, List<Mutation> mutations) {
    super(rng, stats, mutations);
    likelihood = 30;
  }

  private static Opcode[] equalityCmpOpList = {Opcode.IF_EQ, Opcode.IF_NE,
      Opcode.IF_LT, Opcode.IF_GT, Opcode.IF_LE, Opcode.IF_GE} ;
  private static Opcode[] zeroCmpOpList = {Opcode.IF_EQZ, Opcode.IF_NEZ,
      Opcode.IF_LTZ, Opcode.IF_GTZ, Opcode.IF_LEZ, Opcode.IF_GEZ} ;

  @Override
  protected Opcode getModifiedOpcode(MInsn mInsn) {
    Opcode opcode = mInsn.insn.info.opcode;
    isZeroComparator(opcode);
    if(!isZeroComparator(opcode)) {
      int index = Arrays.asList(equalityCmpOpList).indexOf(opcode);
      int length = equalityCmpOpList.length;
      return equalityCmpOpList[(index + 1 + rng.nextInt(length - 1)) % length];
    }
    if(isZeroComparator(opcode)) {
      int index = Arrays.asList(equalityCmpOpList).indexOf(opcode);
      int length = zeroCmpOpList.length;
      return zeroCmpOpList[(index + 1 + rng.nextInt(length - 1)) % length];
    }
    return opcode;
  }

  //Return true if it is a zero comparator. 
  private boolean isZeroComparator(Opcode opcode) {
    if(Opcode.isBetween(opcode, Opcode.IF_EQ, Opcode.IF_LE)) {
      return false;
    }
    return true;
  }
}