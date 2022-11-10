/*
 * Copyright (C) 2022 The Android Open Source Project
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

#include "fault_handler.h"

#include <sys/ucontext.h>

#include "arch/instruction_set.h"
#include "art_method.h"
#include "base/enums.h"
#include "base/hex_dump.h"
#include "base/logging.h"
#include "base/macros.h"
#include "registers_riscv64.h"
#include "runtime_globals.h"
#include "thread-current-inl.h"

extern "C" void art_quick_throw_stack_overflow();
extern "C" void art_quick_throw_null_pointer_exception_from_signal();
extern "C" void art_quick_implicit_suspend();

//
// RISCV64 specific fault handler functions.
//

namespace art {

void FaultManager::GetMethodAndReturnPcAndSp(
    siginfo_t*, void*, ArtMethod**, uintptr_t*, uintptr_t*, bool*) {
  LOG(FATAL) << "FaultManager::GetMethodAndReturnPcAndSp";
}

bool NullPointerHandler::Action(int, siginfo_t*, void*) {
  LOG(FATAL) << "NullPointerHandler::Action";
  return false;
}

bool SuspensionHandler::Action(int, siginfo_t*, void*) {
  LOG(FATAL) << "SuspensionHandler::Action";
  return false;
}

bool StackOverflowHandler::Action(int, siginfo_t*, void*) {
  LOG(FATAL) << "StackOverflowHandler::Action";
  return false;
}

}       // namespace art
