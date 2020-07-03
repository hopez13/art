/*
 * Copyright (C) 2020 The Android Open Source Project
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

#ifndef ART_RUNTIME_INTERPRETER_CFI_ASM_SUPPORT_S_
#define ART_RUNTIME_INTERPRETER_CFI_ASM_SUPPORT_S_

#include "cfi_asm_support.h"

.macro CFI_DEF_CFA_BREG_PLUS_UCONST reg, offset, size
    .if ((\size) < 0)
        .error "Size should be positive"
    .endif
    .if (((\offset) < -0x40) || ((\offset) >= 0x40))
        .error "Unsupported offset"
    .endif
    .if ((\size) < 0x80)
        CFI_DEF_CFA_BREG_PLUS_UCONST_1_1(\reg, \offset, \size)
    .elseif ((\size) < 0x4000)
        CFI_DEF_CFA_BREG_PLUS_UCONST_1_2(\reg, \offset, \size)
    .else
        .error "Unsupported size"
    .endif
.endm

#endif  // ART_RUNTIME_INTERPRETER_CFI_ASM_SUPPORT_S_
