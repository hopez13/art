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

#ifndef ART_DISASSEMBLER_DISASSEMBLER_RISCV64_H_
#define ART_DISASSEMBLER_DISASSEMBLER_RISCV64_H_

#include "disassembler.h"

namespace art {
namespace riscv64 {

#if 0
class CustomDisassembler final : public vixl::aarch64::Disassembler {
 public:
  explicit CustomDisassembler(DisassemblerOptions* options)
      : vixl::aarch64::Disassembler(),
        read_literals_(options->can_read_literals_),
        base_address_(options->base_address_),
        end_address_(options->end_address_),
        options_(options) {
    if (!options->absolute_addresses_) {
      MapCodeAddress(0,
                     reinterpret_cast<const vixl::aarch64::Instruction*>(options->base_address_));
    }
  }

  // Use register aliases in the disassembly.
  void AppendRegisterNameToOutput(const vixl::aarch64::Instruction* instr,
                                  const vixl::aarch64::CPURegister& reg) override;

  // Intercepts the instruction flow captured by the parent method,
  // to specially instrument for particular instruction types.
  void Visit(vixl::aarch64::Metadata* metadata, const vixl::aarch64::Instruction* instr) override;

 private:
  // Improve the disassembly of literal load instructions.
  void VisitLoadLiteralInstr(const vixl::aarch64::Instruction* instr);

  // Improve the disassembly of thread offset.
  void VisitLoadStoreUnsignedOffsetInstr(const vixl::aarch64::Instruction* instr);

  // Improve the disassembly of branch to thunk jumping to pointer from thread entrypoint.
  void VisitUnconditionalBranchInstr(const vixl::aarch64::Instruction* instr);

  void AppendThreadOfsetName(const vixl::aarch64::Instruction* instr);

  // Indicate if the disassembler should read data loaded from literal pools.
  // This should only be enabled if reading the target of literal loads is safe.
  // Here are possible outputs when the option is on or off:
  // read_literals_ | disassembly
  //           true | 0x72681558: 1c000acb  ldr s11, pc+344 (addr 0x726816b0)
  //          false | 0x72681558: 1c000acb  ldr s11, pc+344 (addr 0x726816b0) (3.40282e+38)
  const bool read_literals_;

  DisassemblerOptions* options_;
};
#endif

class DisassemblerRiscv64 final : public Disassembler {
 public:
  explicit DisassemblerRiscv64(DisassemblerOptions* options)
      : Disassembler(options) {}

  size_t Dump(std::ostream& os, const uint8_t* begin) override;
  void Dump(std::ostream& os, const uint8_t* begin, const uint8_t* end) override;

 private:
  class Printer;

  DISALLOW_COPY_AND_ASSIGN(DisassemblerRiscv64);
};

}  // namespace riscv64
}  // namespace art

#endif  // ART_DISASSEMBLER_DISASSEMBLER_RISCV64_H_
