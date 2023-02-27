/*
 * Copyright (C) 2023 The Android Open Source Project
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

#ifndef ART_COMPILER_UTILS_RISCV64_CONSTANTS_RISCV64_H_
#define ART_COMPILER_UTILS_RISCV64_CONSTANTS_RISCV64_H_

#include <android-base/logging.h>

#include <iosfwd>

#include "arch/riscv64/registers_riscv64.h"
#include "base/globals.h"
#include "base/macros.h"

namespace art {
namespace riscv64 {

// Constants used for the decoding or encoding of the individual fields of instructions.
enum InstructionFields {
  kOpcodeShift = 0,
  kOpcodeBits = 7,
  kRs1Shift = 15,
  kRs1Bits = 5,
  kRs2Shift = 20,
  kRs2Bits = 5,
  kRs3Shift = 27,
  kRs3Bits = 5,
  // kRtShift = 16,
  // kRtBits = 5,
  kRdShift = 7,
  kRdBits = 5,
  kShamtShift = 20,
  kShamtBits = 5,
  kFunct2Shift = 25,
  kFunct2Bits = 2,
  kFunct3Shift = 12,
  kFunct3Bits = 3,
  kFunct7Shift = 25,
  kFunct7Bits = 7,

  kIImm12Shift = 20,
  kIImm12Bits = 12,
  kSImm7Shift = 25,
  kSImm7Bits = 7,
  kSImm5Shift = 7,
  kSImm5Bits = 5,
  kUImm20Shift = 12,
  kUImm20Bits = 20,

  /********************************** B-type **************************/
  // |bit 31~25    |24~20 |19~15  |14~12 |11~7         |6~0      |
  // |imm[12|10:5] |rs2   |rs1    |000   |imm[4:1|11]  |1100011  |
  kBImm7Shift = 25,
  kBImm5Shift = 7,
  kBBit12Shift = 5,
  kBBit11Shift = 10,
  kBBit10_5Shift = 4,
  kBBit4_1Shift = 1,

  kGetBit12Mask = 0x800,
  kGetBit11Mask = 0x400,
  kGetBit10_5Mask = 0x3F0,
  kGetBit4_1Mask = 0xF,

  KJImm20shift = 12,
  KJImm20Bits = 20,
  kJBit20Shift = 0,
  kJBit11Shift = 2,
  kJBit19_12Shift = 11,
  kJBit10_1Shift = 9,

  kGetBit20Mask = 0x80000,
  // kGetBit11Mask=0x400,
  kGetBit19_12Mask = 0x7F800,
  kGetBit10_1Mask = 0x3FF,

  /* TODO: To be implemented...
  kFmtShift = 21,
  kFmtBits = 5,
  kFtShift = 16,
  kFtBits = 5,
  kFsShift = 11,
  kFsBits = 5,
  kFdShift = 6,
  kFdBits = 5
  */
};

enum ScaleFactor { TIMES_1 = 0, TIMES_2 = 1, TIMES_4 = 2, TIMES_8 = 3 };

class Instr {
 public:
  static const uint32_t kBreakPointInstruction = 0x00100073;
  // TODO: To be implemented...
  bool IsBreakPoint() {
    return ((*reinterpret_cast<const uint32_t*>(this)) & 0xFC00003F) == kBreakPointInstruction;
  }
  // TODO: To be implemented...
  // Instructions are read out of a code stream. The only way to get a
  // reference to an instruction is to convert a pointer. There is no way
  // to allocate or create instances of class Instr.
  // Use the At(pc) function to create references to Instr.
  static Instr* At(uintptr_t pc) { return reinterpret_cast<Instr*>(pc); }

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(Instr);
};

}  // namespace riscv64
}  // namespace art

#endif  // ART_COMPILER_UTILS_RISCV64_CONSTANTS_RISCV64_H_
