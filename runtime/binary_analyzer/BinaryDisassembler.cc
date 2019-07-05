/*
 * Copyright (C) 2019 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreLOGed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "arch/instruction_set.h"
#include "base/logging.h"
#include <disassembler.h>
#include <disassembler_x86.h>
#include "BinaryDisassembler.h"
#include <dlfcn.h>
#include <cctype>
#include <sstream>

#include <cstdio>
#include <cstring>
#include <limits>
#include <thread.h>

namespace art {

BinaryDisassembler::BinaryDisassembler(InstructionSet insn_set) {
   if (disassembler_ == nullptr) {
      const uint8_t* base_address = nullptr;
      const uint8_t* end_address  = nullptr;
      disassembler_ = std::unique_ptr<Disassembler>((*create_disassembler)(
                      insn_set,
                      new DisassemblerOptions(/* absolute_addresses */ false,
                                           base_address,
                                           end_address,
                                           /* can_read_literals */ true,
                                           Is64BitInstructionSet(insn_set)
                                           ? &Thread::DumpThreadOffset<PointerSize::k64> : 
				           &Thread::DumpThreadOffset<PointerSize::k32>)));
   }
   
}

BinaryDisassembler::~BinaryDisassembler() {
    disassembler_.reset();
}

}  // namespace art
