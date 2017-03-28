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

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#include <iostream>
#include <vector>

#include "android-base/stringprintf.h"

#include "base/logging.h"
#include "base/macros.h"
#include "bytecode_utils.h"
#include "dex_file.h"
#include "dex_instruction.h"
#include "jit/jit.h"
#include "jni.h"
#include "native_stack_dump.h"
#include "jvmti.h"
#include "runtime.h"
#include "scoped_thread_state_change-inl.h"
#include "thread-inl.h"
#include "thread_list.h"

#include "ti-agent/common_helper.h"
#include "ti-agent/common_load.h"

namespace art {
namespace Test983SourceTransformVerify {

// The hook we are using.
void JNICALL CheckDexFileHook(jvmtiEnv* jvmti_env ATTRIBUTE_UNUSED,
                              JNIEnv* jni_env ATTRIBUTE_UNUSED,
                              jclass class_being_redefined,
                              jobject loader ATTRIBUTE_UNUSED,
                              const char* name,
                              jobject protection_domain ATTRIBUTE_UNUSED,
                              jint class_data_len,
                              const unsigned char* class_data,
                              jint* new_class_data_len ATTRIBUTE_UNUSED,
                              unsigned char** new_class_data ATTRIBUTE_UNUSED) {
  if (class_being_redefined == nullptr) {
    // Something got loaded concurrently. Just ignore it for now.
    return;
  }
  std::cout << "Dex file hook for " << name << std::endl;
  if (IsJVM()) {
    return;
  }
  std::string error;
  std::unique_ptr<const DexFile> dex(DexFile::Open(class_data,
                                                   class_data_len,
                                                   "fake_location.dex",
                                                   0,
                                                   nullptr,
                                                   true,
                                                   true,
                                                   &error));
  if (dex.get() == nullptr) {
    std::cout << "Failed to verify dex file for " << name << " because " << error << std::endl;
    return;
  }
  for (uint32_t i = 0; i < dex->NumClassDefs(); i++) {
    const DexFile::ClassDef& def = dex->GetClassDef(i);
    const uint8_t* data_item = dex->GetClassData(def);
    if (data_item == nullptr) {
      continue;
    }
    ClassDataItemIterator it(*dex, data_item);
    while (it.HasNextStaticField()) {
      it.Next();
    }
    while (it.HasNextInstanceField()) {
      it.Next();
    }
    while (it.HasNextVirtualMethod() || it.HasNextDirectMethod()) {
      const DexFile::CodeItem* code_item = it.GetMethodCodeItem();
      if (code_item != nullptr) {
        for (CodeItemIterator code_it(*code_item); !code_it.Done(); code_it.Advance()) {
          Instruction* inst = const_cast<Instruction*>(&code_it.CurrentInstruction());
          switch (inst->Opcode()) {
            case Instruction::RETURN_VOID_NO_BARRIER:
            case Instruction::IGET_QUICK:
            case Instruction::IGET_WIDE_QUICK:
            case Instruction::IGET_OBJECT_QUICK:
            case Instruction::IGET_BOOLEAN_QUICK:
            case Instruction::IGET_BYTE_QUICK:
            case Instruction::IGET_CHAR_QUICK:
            case Instruction::IGET_SHORT_QUICK:
            case Instruction::IPUT_BOOLEAN_QUICK:
            case Instruction::IPUT_BYTE_QUICK:
            case Instruction::IPUT_CHAR_QUICK:
            case Instruction::IPUT_SHORT_QUICK:
            case Instruction::IPUT_WIDE_QUICK:
            case Instruction::IPUT_OBJECT_QUICK:
            case Instruction::INVOKE_VIRTUAL_QUICK:
            case Instruction::INVOKE_VIRTUAL_RANGE_QUICK:
              std::cout << "Unexpected instruction found: "
                        << inst->DumpString(dex.get()) << std::endl;
              break;
            default:
              break;
          }
        }
      }
      it.Next();
    }
  }
}

// Get all capabilities except those related to retransformation.
jint OnLoad(JavaVM* vm,
            char* options ATTRIBUTE_UNUSED,
            void* reserved ATTRIBUTE_UNUSED) {
  if (vm->GetEnv(reinterpret_cast<void**>(&jvmti_env), JVMTI_VERSION_1_0)) {
    printf("Unable to get jvmti env!\n");
    return 1;
  }
  SetAllCapabilities(jvmti_env);
  jvmtiEventCallbacks cb;
  memset(&cb, 0, sizeof(cb));
  cb.ClassFileLoadHook = CheckDexFileHook;
  if (jvmti_env->SetEventCallbacks(&cb, sizeof(cb)) != JVMTI_ERROR_NONE) {
    printf("Unable to set class file load hook cb!\n");
    return 1;
  }
  return 0;
}


}  // namespace Test983SourceTransformVerify
}  // namespace art
