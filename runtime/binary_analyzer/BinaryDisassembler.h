/*
 * Copyright (C) 2019 The Android Open Source Project
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

#ifndef ART_RUNTIME_BINARY_ANALYZER_BINARYDISASSEMBLER_H_
#define ART_RUNTIME_BINARY_ANALYZER_BINARYDISASSEMBLER_H_

#include "arch/instruction_set.h"
#include <cstddef>
#include <disassembler.h>
#include <disassembler_x86.h>


namespace art {


/**
 * @brief The Disassembler class is a wrapper around the ART disassembly library.
 * The Disassembler vaidates each x86 instruction and returns required fields
 * of instruction mnemonic, instruction size , instruction prefixes & opcode.
 */
class BinaryDisassembler {
 public:
  explicit BinaryDisassembler(InstructionSet insn_set);
  ~BinaryDisassembler();

  /**
   * @brief Returns the address of the head of the stream (that is, the address of the
   * stream that the user indicated when the last Seek operation was performed).
   *
   * @return Address of the head of the stream.
   */
  uint64_t GetAddress() const {
    return address_;
  }

  /**
   * @brief Returns a pointer corresponding to the head of the binary stream.
   *
   * @return Pointer to the head of the strema.
   */
  const uint8_t* GetPtr() const {
    return ptr_;
  }

  /**
   * @brief Checks if the Disassembler was constructed successfully. The constructor
   * will fail if the library was not configured for the requested architecture.
   *
   * @return true if the Disassembler is ready for use; false otherwise.
   */
   bool IsDisassemblerValid() const {
    if (disassembler_ == nullptr) {
       LOG(ERROR) << "Invalid Disassembler !" ;
       return false;
    } 
    return true;
  }

  /**
   * @brief Positions the iterator head at ptr and sets the user-specified address to ptr.
   */
  void Seek(const uint8_t* ptr) {
    Seek(ptr, reinterpret_cast<uint64_t>(ptr));
  }

  /**
   * @brief Positions the iterator head at ptr and sets the user-specified address.
   */
  void Seek(const uint8_t* ptr, uint64_t address) {
    ptr_ = ptr;
    address_ = address;
  }
  
  art::x86::x86_instr GetInstructionDetails(const uint8_t* instr)  {
       art::x86::x86_instr insn_x86 = {};
       art::x86::DisassemblerX86* disasm = static_cast<art::x86::DisassemblerX86 *>(disassembler_.get());
       (disasm)->GetInstructionDetails(instr, &insn_x86);
       return insn_x86;
  }
  
  size_t DumpInstruction(std::ostream& os, const uint8_t* instr);
  
 private:
  uint64_t address_;
  const uint8_t* ptr_;
  std::unique_ptr<Disassembler> disassembler_ = nullptr;
 private:
  DISALLOW_COPY_AND_ASSIGN(BinaryDisassembler);
};

}  // namespace art

#endif  // ART_RUNTIME_BINARY_ANALYZER_BINARYDISASSEMBLER_H_
