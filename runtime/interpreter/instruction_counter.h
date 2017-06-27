/*
 * Copyright (C) 2008 The Android Open Source Project
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

#ifndef ART_RUNTIME_INTERPRETER_INSTRUCTION_COUNTER_H_
#define ART_RUNTIME_INTERPRETER_INSTRUCTION_COUNTER_H_

#include "instrumentation.h"

namespace art {
namespace instrumentation {

// ClassInitializerInstrumentationListener, used in aot compiler to count instructions executed
// in <clinit>.
class CIInstrumentationListener FINAL: public instrumentation::InstrumentationListener {
 public:
  CIInstrumentationListener() : counter(0), threashold_(100000) {}

  explicit CIInstrumentationListener(bool abort, int threashold)
      : counter(0), abort_(abort), threashold_(threashold) { }

  virtual ~CIInstrumentationListener() {}

  void MethodEntered(Thread *thread ATTRIBUTE_UNUSED,
                     Handle<mirror::Object> this_object ATTRIBUTE_UNUSED,
                     ArtMethod *method ATTRIBUTE_UNUSED,
                     uint32_t dex_pc ATTRIBUTE_UNUSED)
  OVERRIDE REQUIRES_SHARED(Locks::mutator_lock_) {
  }

  virtual void MethodExited(Thread* thread ATTRIBUTE_UNUSED,
                            Handle<mirror::Object> this_object ATTRIBUTE_UNUSED,
                            ArtMethod* method ATTRIBUTE_UNUSED,
                            uint32_t dex_pc ATTRIBUTE_UNUSED,
                            Handle<mirror::Object> return_value ATTRIBUTE_UNUSED)
  OVERRIDE REQUIRES_SHARED(Locks::mutator_lock_) {
  }

  void MethodExited(Thread *thread ATTRIBUTE_UNUSED,
                    Handle<mirror::Object> this_object ATTRIBUTE_UNUSED,
                    ArtMethod *method ATTRIBUTE_UNUSED,
                    uint32_t dex_pc ATTRIBUTE_UNUSED,
                    const JValue &return_value ATTRIBUTE_UNUSED)
  OVERRIDE REQUIRES_SHARED(Locks::mutator_lock_) {
  }

  void MethodUnwind(Thread *thread ATTRIBUTE_UNUSED,
                    Handle<mirror::Object> this_object ATTRIBUTE_UNUSED,
                    ArtMethod *method ATTRIBUTE_UNUSED,
                    uint32_t dex_pc ATTRIBUTE_UNUSED)
  OVERRIDE REQUIRES_SHARED(Locks::mutator_lock_) {
  }

  void DexPcMoved(Thread *thread ATTRIBUTE_UNUSED,
                  Handle<mirror::Object> this_object ATTRIBUTE_UNUSED,
                  ArtMethod *method ATTRIBUTE_UNUSED,
                  uint32_t new_dex_pc ATTRIBUTE_UNUSED)
  OVERRIDE REQUIRES_SHARED(Locks::mutator_lock_) {
    counter++;
    if (abort_ && counter > threashold_) {
      Runtime::Current()->
          AbortTransactionAndThrowAbortError(Thread::Current(), "Clinit takes too long time - "
          "Threashold: " + std::to_string(threashold_));
    }
  }

  void FieldRead(Thread *thread ATTRIBUTE_UNUSED,
                 Handle<mirror::Object> this_object ATTRIBUTE_UNUSED,
                 ArtMethod *method ATTRIBUTE_UNUSED,
                 uint32_t dex_pc ATTRIBUTE_UNUSED,
                 ArtField *field ATTRIBUTE_UNUSED)
  OVERRIDE REQUIRES_SHARED(Locks::mutator_lock_) {
  }

  virtual void FieldWritten(Thread* thread ATTRIBUTE_UNUSED,
                            Handle<mirror::Object> this_object ATTRIBUTE_UNUSED,
                            ArtMethod* method ATTRIBUTE_UNUSED,
                            uint32_t dex_pc ATTRIBUTE_UNUSED,
                            ArtField* field ATTRIBUTE_UNUSED,
                            Handle<mirror::Object> field_value ATTRIBUTE_UNUSED)
      OVERRIDE REQUIRES_SHARED(Locks::mutator_lock_) {
      }


  void FieldWritten(Thread *thread ATTRIBUTE_UNUSED,
                    Handle<mirror::Object> this_object ATTRIBUTE_UNUSED,
                    ArtMethod *method ATTRIBUTE_UNUSED,
                    uint32_t dex_pc ATTRIBUTE_UNUSED,
                    ArtField *field ATTRIBUTE_UNUSED,
                    const JValue &field_value ATTRIBUTE_UNUSED)
  OVERRIDE REQUIRES_SHARED(Locks::mutator_lock_) { }

  void ExceptionCaught(Thread *thread ATTRIBUTE_UNUSED,
                       Handle<mirror::Throwable> exception_object ATTRIBUTE_UNUSED)
  OVERRIDE REQUIRES_SHARED(Locks::mutator_lock_) {
  }

  void Branch(Thread *thread ATTRIBUTE_UNUSED,
              ArtMethod *method ATTRIBUTE_UNUSED,
              uint32_t dex_pc ATTRIBUTE_UNUSED,
              int32_t dex_pc_offset ATTRIBUTE_UNUSED)
  OVERRIDE REQUIRES_SHARED(Locks::mutator_lock_) {
  }

  void InvokeVirtualOrInterface(Thread *thread ATTRIBUTE_UNUSED,
                                Handle<mirror::Object> this_object ATTRIBUTE_UNUSED,
                                ArtMethod *caller ATTRIBUTE_UNUSED,
                                uint32_t dex_pc ATTRIBUTE_UNUSED,
                                ArtMethod *callee ATTRIBUTE_UNUSED)
  OVERRIDE REQUIRES_SHARED(Locks::mutator_lock_) {
  }

  uint32_t getCount() {
    return counter.load();
  }

  void resetCount() {
    counter = 0;
  }

 private:
  std::atomic<uint32_t> counter;
  bool abort_;
  const uint32_t threashold_;
};

}  // namespace instrumentation
}  // namespace art

#endif  // ART_RUNTIME_INTERPRETER_INSTRUCTION_COUNTER_H_
