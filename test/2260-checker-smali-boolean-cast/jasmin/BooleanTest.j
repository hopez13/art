; Copyright (C) 2023 The Android Open Source Project
;
; Licensed under the Apache License, Version 2.0 (the "License");
; you may not use this file except in compliance with the License.
; You may obtain a copy of the License at
;
;      http://www.apache.org/licenses/LICENSE-2.0
;
; Unless required by applicable law or agreed to in writing, software
; distributed under the License is distributed on an "AS IS" BASIS,
; WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
; See the License for the specific language governing permissions and
; limitations under the License.

.class public BooleanTest
.super java/lang/Object

.method public <init>()V
  .limit stack 1
  .limit locals 1
  aload_0
  invokespecial java/lang/Object/<init>()V
  return
.end method

;; CHECK-START: boolean BooleanTest.$noinline$setGetBooleanConst0() load_store_elimination (before)
;; CHECK: StaticFieldSet field_name:Main.field field_type:Bool
;; CHECK: StaticFieldGet field_name:Main.field field_type:Bool

;; CHECK-START: boolean BooleanTest.$noinline$setGetBooleanConst0() load_store_elimination (after)
;; CHECK-NOT: StaticFieldGet field_name:Main.field field_type:Bool

; Implicit type conversion special case 0
;; CHECK-START: boolean BooleanTest.$noinline$setGetBooleanConst0() load_store_elimination (after)
;; CHECK: <<Const0:i\d+>> IntConstant 0
;; CHECK:                 Return [<<Const0>>]
.method public static $noinline$setGetBooleanConst0()Z
    .limit stack 1
    ldc 0
    putstatic Main/field Z
    getstatic Main/field Z
    ireturn
.end method


;; CHECK-START: boolean BooleanTest.$noinline$setGetBooleanConst1() load_store_elimination (before)
;; CHECK: StaticFieldSet field_name:Main.field field_type:Bool
;; CHECK: StaticFieldGet field_name:Main.field field_type:Bool

;; CHECK-START: boolean BooleanTest.$noinline$setGetBooleanConst1() load_store_elimination (after)
;; CHECK-NOT: StaticFieldGet field_name:Main.field field_type:Bool

;; CHECK-START: boolean BooleanTest.$noinline$setGetBooleanConst1() load_store_elimination (after)
;; CHECK: StaticFieldSet field_name:Main.field field_type:Bool
;; CHECK: TypeConversion

;; CHECK-START: boolean BooleanTest.$noinline$setGetBooleanConst1() constant_folding$before_codegen (after)
;; CHECK: <<Const1:i\d+>> IntConstant 1
;; CHECK:                 Return [<<Const1>>]
.method public static $noinline$setGetBooleanConst1()Z
    .limit stack 1
    ldc 1
    putstatic Main/field Z
    getstatic Main/field Z
    ireturn
.end method

;; CHECK-START: boolean BooleanTest.$noinline$setGetBooleanConst2() load_store_elimination (before)
;; CHECK: StaticFieldSet field_name:Main.field field_type:Bool
;; CHECK: StaticFieldGet field_name:Main.field field_type:Bool

;; CHECK-START: boolean BooleanTest.$noinline$setGetBooleanConst2() load_store_elimination (after)
;; CHECK-NOT: StaticFieldGet field_name:Main.field field_type:Bool

;; CHECK-START: boolean BooleanTest.$noinline$setGetBooleanConst2() load_store_elimination (after)
;; CHECK: StaticFieldSet field_name:Main.field field_type:Bool
;; CHECK: TypeConversion

;; CHECK-START: boolean BooleanTest.$noinline$setGetBooleanConst2() constant_folding$before_codegen (after)
;; CHECK: <<Const2:i\d+>> IntConstant 2
;; CHECK:                 Return [<<Const2>>]
.method public static $noinline$setGetBooleanConst2()Z
    .limit stack 1
    ldc 2
    putstatic Main/field Z
    getstatic Main/field Z
    ireturn
.end method

;; CHECK-START: boolean BooleanTest.$noinline$setGetBooleanConst255() load_store_elimination (before)
;; CHECK: StaticFieldSet field_name:Main.field field_type:Bool
;; CHECK: StaticFieldGet field_name:Main.field field_type:Bool

;; CHECK-START: boolean BooleanTest.$noinline$setGetBooleanConst255() load_store_elimination (after)
;; CHECK-NOT: StaticFieldGet field_name:Main.field field_type:Bool

;; CHECK-START: boolean BooleanTest.$noinline$setGetBooleanConst255() load_store_elimination (after)
;; CHECK: StaticFieldSet field_name:Main.field field_type:Bool
;; CHECK: TypeConversion

;; CHECK-START: boolean BooleanTest.$noinline$setGetBooleanConst255() constant_folding$before_codegen (after)
;; CHECK: <<Const255:i\d+>> IntConstant 255
;; CHECK:                   Return [<<Const255>>]
.method public static $noinline$setGetBooleanConst255()Z
    .limit stack 1
    ldc 255
    putstatic Main/field Z
    getstatic Main/field Z
    ireturn
.end method

;; CHECK-START: boolean BooleanTest.$noinline$setGetBooleanConst256() load_store_elimination (before)
;; CHECK: StaticFieldSet field_name:Main.field field_type:Bool
;; CHECK: StaticFieldGet field_name:Main.field field_type:Bool

;; CHECK-START: boolean BooleanTest.$noinline$setGetBooleanConst256() load_store_elimination (after)
;; CHECK-NOT: StaticFieldGet field_name:Main.field field_type:Bool

;; CHECK-START: boolean BooleanTest.$noinline$setGetBooleanConst256() load_store_elimination (after)
;; CHECK: StaticFieldSet field_name:Main.field field_type:Bool
;; CHECK: TypeConversion

;; CHECK-START: boolean BooleanTest.$noinline$setGetBooleanConst256() constant_folding$before_codegen (after)
;; CHECK: <<Const0:i\d+>> IntConstant 0
;; CHECK:                 Return [<<Const0>>]
.method public static $noinline$setGetBooleanConst256()Z
    .limit stack 1
    ldc 256
    putstatic Main/field Z
    getstatic Main/field Z
    ireturn
.end method

;; CHECK-START: boolean BooleanTest.$noinline$setGetBooleanConst257() load_store_elimination (before)
;; CHECK: StaticFieldSet field_name:Main.field field_type:Bool
;; CHECK: StaticFieldGet field_name:Main.field field_type:Bool

;; CHECK-START: boolean BooleanTest.$noinline$setGetBooleanConst257() load_store_elimination (after)
;; CHECK-NOT: StaticFieldGet field_name:Main.field field_type:Bool

;; CHECK-START: boolean BooleanTest.$noinline$setGetBooleanConst257() load_store_elimination (after)
;; CHECK: StaticFieldSet field_name:Main.field field_type:Bool
;; CHECK: TypeConversion

;; CHECK-START: boolean BooleanTest.$noinline$setGetBooleanConst257() constant_folding$before_codegen (after)
;; CHECK: <<Const1:i\d+>> IntConstant 1
;; CHECK:                 Return [<<Const1>>]
.method public static $noinline$setGetBooleanConst257()Z
    .limit stack 1
    ldc 257
    putstatic Main/field Z
    getstatic Main/field Z
    ireturn
.end method

;; CHECK-START: boolean BooleanTest.$noinline$setGetBooleanConst511() load_store_elimination (before)
;; CHECK: StaticFieldSet field_name:Main.field field_type:Bool
;; CHECK: StaticFieldGet field_name:Main.field field_type:Bool

;; CHECK-START: boolean BooleanTest.$noinline$setGetBooleanConst511() load_store_elimination (after)
;; CHECK-NOT: StaticFieldGet field_name:Main.field field_type:Bool

;; CHECK-START: boolean BooleanTest.$noinline$setGetBooleanConst511() load_store_elimination (after)
;; CHECK: StaticFieldSet field_name:Main.field field_type:Bool
;; CHECK: TypeConversion

;; CHECK-START: boolean BooleanTest.$noinline$setGetBooleanConst511() constant_folding$before_codegen (after)
;; CHECK: <<Const255:i\d+>> IntConstant 255
;; CHECK:                 Return [<<Const255>>]
.method public static $noinline$setGetBooleanConst511()Z
    .limit stack 1
    ldc 511
    putstatic Main/field Z
    getstatic Main/field Z
    ireturn
.end method

;; CHECK-START: boolean BooleanTest.$noinline$setGetBooleanConst512() load_store_elimination (before)
;; CHECK: StaticFieldSet field_name:Main.field field_type:Bool
;; CHECK: StaticFieldGet field_name:Main.field field_type:Bool

;; CHECK-START: boolean BooleanTest.$noinline$setGetBooleanConst512() load_store_elimination (after)
;; CHECK-NOT: StaticFieldGet field_name:Main.field field_type:Bool

;; CHECK-START: boolean BooleanTest.$noinline$setGetBooleanConst512() load_store_elimination (after)
;; CHECK: StaticFieldSet field_name:Main.field field_type:Bool
;; CHECK: TypeConversion

;; CHECK-START: boolean BooleanTest.$noinline$setGetBooleanConst512() constant_folding$before_codegen (after)
;; CHECK: <<Const0:i\d+>> IntConstant 0
;; CHECK:                 Return [<<Const0>>]
.method public static $noinline$setGetBooleanConst512()Z
    .limit stack 1
    ldc 512
    putstatic Main/field Z
    getstatic Main/field Z
    ireturn
.end method

;; CHECK-START: boolean BooleanTest.$noinline$setGetBooleanConst513() load_store_elimination (before)
;; CHECK: StaticFieldSet field_name:Main.field field_type:Bool
;; CHECK: StaticFieldGet field_name:Main.field field_type:Bool

;; CHECK-START: boolean BooleanTest.$noinline$setGetBooleanConst513() load_store_elimination (after)
;; CHECK-NOT: StaticFieldGet field_name:Main.field field_type:Bool

;; CHECK-START: boolean BooleanTest.$noinline$setGetBooleanConst513() load_store_elimination (after)
;; CHECK: StaticFieldSet field_name:Main.field field_type:Bool
;; CHECK: TypeConversion

;; CHECK-START: boolean BooleanTest.$noinline$setGetBooleanConst513() constant_folding$before_codegen (after)
;; CHECK: <<Const1:i\d+>> IntConstant 1
;; CHECK:                 Return [<<Const1>>]
.method public static $noinline$setGetBooleanConst513()Z
    .limit stack 1
    ldc 513
    putstatic Main/field Z
    getstatic Main/field Z
    ireturn
.end method

;; CHECK-START: boolean BooleanTest.$noinline$setGetBoolean(int) load_store_elimination (before)
;; CHECK: StaticFieldSet field_name:Main.field field_type:Bool
;; CHECK: StaticFieldGet field_name:Main.field field_type:Bool

;; CHECK-START: boolean BooleanTest.$noinline$setGetBoolean(int) load_store_elimination (after)
;; CHECK-NOT: StaticFieldGet field_name:Main.field field_type:Bool

;; CHECK-START: boolean BooleanTest.$noinline$setGetBoolean(int) load_store_elimination (after)
;; CHECK: StaticFieldSet field_name:Main.field field_type:Bool
;; CHECK: TypeConversion

; We cannot eliminate the TypeConversion in this case.
;; CHECK-START: boolean BooleanTest.$noinline$setGetBoolean(int) constant_folding$before_codegen (after)
;; CHECK: TypeConversion
.method public static $noinline$setGetBoolean(I)Z
    .limit stack 1
    iload_0
    putstatic Main/field Z
    getstatic Main/field Z
    ireturn
.end method