package dexfuzz.program.mutators;

import dexfuzz.program.MInsn;
import dexfuzz.rawdex.Opcode;
import java.util.List;

public class RandomBranchChanger extends IfBranchChanger {
  @Override
  protected Opcode getModifiedOpcode(MInsn mInsn) {
    Opcode oldOpcode = mInsn.insn.info.opcode;
    Opcode newOpcode = oldOpcode;
    isZeroComparator(oldOpcode);
    //store the opcodes in two different arrays.
    Opcode[] equalityCmpOpList = {Opcode.IF_EQ, Opcode.IF_NE,
        Opcode.IF_LT, Opcode.IF_GT, Opcode.IF_LE, Opcode.IF_GE} ;
    Opcode[] zeroCmpOpList = {Opcode.IF_EQZ, Opcode.IF_NEZ,
        Opcode.IF_LTZ, Opcode.IF_GTZ, Opcode.IF_LEZ, Opcode.IF_GEZ} ;
    //run the loop until a new opcode is found
    while (newOpcode == oldOpcode) { 
      if(!isZeroComparator(oldOpcode)) {
        newOpcode = equalityCmpOpList[rng.nextInt(equalityCmpOpList.length)];
      }
      if(isZeroComparator(oldOpcode)) {
        newOpcode = equalityCmpOpList[rng.nextInt(zeroCmpOpList.length)];
      }
    }
    return oldOpcode;
    
  }
  //return 
  private boolean isZeroComparator(Opcode opcode) {
    if(Opcode.isBetween(opcode, Opcode.IF_EQ, Opcode.IF_LE)) {
      return false;
    }
    return true;
    
  }
}
