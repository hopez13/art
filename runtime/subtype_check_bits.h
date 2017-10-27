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

#ifndef ART_RUNTIME_SUBTYPE_CHECK_BITS_H_
#define ART_RUNTIME_SUBTYPE_CHECK_BITS_H_

#include "base/bit_string.h"
#include "base/bit_struct.h"
#include "base/bit_utils.h"

namespace art {

/**
 *                                 Variable
 *                                     |
 *        <---- up to 23 bits ---->    v               +---> 1 bit
 *                                                     |
 *  +-------------------------+--------+-----------+---++
 *  |             Bitstring                        |    |
 *  +-------------------------+--------+-----------+    |
 *  |      Path To Root       |  Next  | (unused)  | OF |
 *  +---+---------------------+--------+           |    |
 *  |   |    |    |    | ...  |        | (0....0)  |    |
 *  +---+---------------------+--------+-----------+----+
 * MSB                                                LSB
 */
BITSTRUCT_DEFINE_START(SubtypeCheckBits, /*size*/ BitString::BitStructSizeOf() + 1u)
  BitStructUint</*lsb*/0, /*width*/1> overflow_;
  BitStructField<BitString, /*lsb*/1> bitstring_;
BITSTRUCT_DEFINE_END(SubtypeCheckBits);

}  // namespace art

#endif  // ART_RUNTIME_SUBTYPE_CHECK_BITS_H_
