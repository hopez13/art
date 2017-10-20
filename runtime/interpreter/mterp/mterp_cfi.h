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

#ifndef ART_RUNTIME_INTERPRETER_MTERP_MTERP_CFI_H_
#define ART_RUNTIME_INTERPRETER_MTERP_MTERP_CFI_H_

/*
 * Special cfi directives to identify the location/value of the Dalivk PC.  Will
 * be emitted as .cfi_escape expressions.  To avoid collisions with other uses of
 * .cfi_escape, all ART .cfi directives will begin with ART_FRAME.
 *
 * The Dalvik PC will be either in a register or somewhere in the frame.  Because it is
 * often incremented during processing to load argument words, the directives here will
 * also allow specifying an offset to apply to the Dalvik PC to allow identification of the
 * beginning of the instrucion.
 *
 * Supported directives:
 *
 *  // Identify the register holding the Dalvik PC and set/reset offset to zer0.
 * .cfi_escape ART_FRAME,DEF_DEX_PC_REG,register
 *
 *  // Identify the register holding the address of the Dalvik PC and set offset to zero.
 *  //  Used for cases in which the Dex PC is held in stack memory rather than a dedicated
 *  // register.
 *  .cfi_escape ART_FRAME,DEF_DEX_PC_ADDR,register
 * 
 *  // Apply an offset to the current Dex PC by subtraction to yeild the address of the beginning
 *  // of the currently Dex instruction.  This is used to undo intermediate Dex PC values
 *  // that occur while instruction immediates are loaded.  Note: offset is in bytes and is
 *  // relative to the previous value.  Offset is limited to 0..255.
 * .cfi_escape ART_FRAME,ADJUST_DEX_PC,offset
 *
 *  // Similar to ADJUST_DEX_PC, but instead of an immediate offset, the adjustment amount
 *  // (in bytes) is held in a register.
 *  .cfi_escape ART_FRAME,ADJUST_DEX_PC_REG,register
 */

#define ART_FRAME 0x67
#define DEF_DEX_PC_REG 1
#define DEF_DEX_PC_ADDR 2
#define ADJUST_DEX_PC 3
#define ADJUST_DEX_PC_REG 4

#endif  // ART_RUNTIME_INTERPRETER_MTERP_MTERP_CFI_H_
