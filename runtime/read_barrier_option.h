/*
 * Copyright (C) 2014 The Android Open Source Project
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

#ifndef ART_RUNTIME_READ_BARRIER_OPTION_H_
#define ART_RUNTIME_READ_BARRIER_OPTION_H_
namespace art {

// Options for performing a read barrier or not.
//
// Besides disabled GC and GC's internal usage, there are a few cases where the read
// barrier is unnecessary and can be avoided to reduce code size and improve performance.
// In the following cases, the result of the operation or chain of operations shall be the
// same whether we go through the from-space or to-space:
//
// 1. We're reading a reference known to point to an object in the non-moving space.
//    (For example the java.lang.Class.class or boot image objects.)
// 2. We're reading the reference only for comparison with null.
// 3. We're reading a reference to a holder from which we shall read
//      - constant primitive value, or
//      - constant reference for comparison with null, or,
//      - constant reference known to point to an object in the non-moving space, or
//      - constant references in a chain leading to one of the above purposes;
//        the entire chain needs to be read without read barrier.
//    The term "constant" refers to values set to their final value between allocating
//    the holder and the next opportunity for the holder to be moved by the GC, i.e.
//    before the first suspend point after the allocation, or seen by another thread.
//    This includes several fields in the Class object, such as the primitive type or
//    component type but not the superclass.
enum ReadBarrierOption {
  kWithReadBarrier,     // Perform a read barrier.
  kWithoutReadBarrier,  // Don't perform a read barrier.
};

}  // namespace art

#endif  // ART_RUNTIME_READ_BARRIER_OPTION_H_
