# Copyright (C) 2023 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

.class public LBooleanTest;

.super Ljava/lang/Object;

# The setGetBoolean methods store a integer value in a boolean field, load that boolean field,
# and then return the boolean value. There are methods with particularly interesting
# constant integer values, as well as one that uses a parameter.

## CHECK-START: boolean BooleanTest.setGetBooleanConst0() load_store_elimination (before)
## CHECK: StaticFieldSet field_name:Main.field field_type:Bool
## CHECK: StaticFieldGet field_name:Main.field field_type:Bool

## CHECK-START: boolean BooleanTest.setGetBooleanConst0() load_store_elimination (after)
## CHECK-NOT: StaticFieldGet field_name:Main.field field_type:Bool

# Implicit type conversion special case 0
## CHECK-START: boolean BooleanTest.setGetBooleanConst0() load_store_elimination (after)
## CHECK-NOT: TypeConversion
.method public static setGetBooleanConst0()Z
    .registers 1
    const/4 v0, 0x0
    sput-boolean v0, LMain;->field:Z
    sget-boolean v0, LMain;->field:Z
    return v0
.end method

## CHECK-START: boolean BooleanTest.setGetBooleanConst1() load_store_elimination (before)
## CHECK: StaticFieldSet field_name:Main.field field_type:Bool
## CHECK: StaticFieldGet field_name:Main.field field_type:Bool

## CHECK-START: boolean BooleanTest.setGetBooleanConst1() load_store_elimination (after)
## CHECK-NOT: StaticFieldGet field_name:Main.field field_type:Bool

# Implicit type conversion special case 1
## CHECK-START: boolean BooleanTest.setGetBooleanConst0() load_store_elimination (after)
## CHECK-NOT: TypeConversion
.method public static setGetBooleanConst1()Z
    .registers 1
    const/4 v0, 0x1
    sput-boolean v0, LMain;->field:Z
    sget-boolean v0, LMain;->field:Z
    return v0
.end method

## CHECK-START: boolean BooleanTest.setGetBooleanConst2() load_store_elimination (before)
## CHECK: StaticFieldSet field_name:Main.field field_type:Bool
## CHECK: StaticFieldGet field_name:Main.field field_type:Bool

## CHECK-START: boolean BooleanTest.setGetBooleanConst2() load_store_elimination (after)
## CHECK-NOT: StaticFieldGet field_name:Main.field field_type:Bool

## CHECK-START: boolean BooleanTest.setGetBooleanConst2() load_store_elimination (after)
## CHECK: StaticFieldSet field_name:Main.field field_type:Bool
## CHECK: TypeConversion
.method public static setGetBooleanConst2()Z
    .registers 1
    const/4 v0, 0x2
    sput-boolean v0, LMain;->field:Z
    sget-boolean v0, LMain;->field:Z
    return v0
.end method

## CHECK-START: boolean BooleanTest.setGetBooleanConst255() load_store_elimination (before)
## CHECK: StaticFieldSet field_name:Main.field field_type:Bool
## CHECK: StaticFieldGet field_name:Main.field field_type:Bool

## CHECK-START: boolean BooleanTest.setGetBooleanConst255() load_store_elimination (after)
## CHECK-NOT: StaticFieldGet field_name:Main.field field_type:Bool

## CHECK-START: boolean BooleanTest.setGetBooleanConst255() load_store_elimination (after)
## CHECK: StaticFieldSet field_name:Main.field field_type:Bool
## CHECK: TypeConversion
.method public static setGetBooleanConst255()Z
    .registers 1
    const/16 v0, 0xFF
    sput-boolean v0, LMain;->field:Z
    sget-boolean v0, LMain;->field:Z
    return v0
.end method

## CHECK-START: boolean BooleanTest.setGetBooleanConst256() load_store_elimination (before)
## CHECK: StaticFieldSet field_name:Main.field field_type:Bool
## CHECK: StaticFieldGet field_name:Main.field field_type:Bool

## CHECK-START: boolean BooleanTest.setGetBooleanConst256() load_store_elimination (after)
## CHECK-NOT: StaticFieldGet field_name:Main.field field_type:Bool

## CHECK-START: boolean BooleanTest.setGetBooleanConst256() load_store_elimination (after)
## CHECK: StaticFieldSet field_name:Main.field field_type:Bool
## CHECK: TypeConversion
.method public static setGetBooleanConst256()Z
    .registers 1
    const/16 v0, 0x100
    sput-boolean v0, LMain;->field:Z
    sget-boolean v0, LMain;->field:Z
    return v0
.end method

## CHECK-START: boolean BooleanTest.setGetBooleanConst257() load_store_elimination (before)
## CHECK: StaticFieldSet field_name:Main.field field_type:Bool
## CHECK: StaticFieldGet field_name:Main.field field_type:Bool

## CHECK-START: boolean BooleanTest.setGetBooleanConst257() load_store_elimination (after)
## CHECK-NOT: StaticFieldGet field_name:Main.field field_type:Bool

## CHECK-START: boolean BooleanTest.setGetBooleanConst257() load_store_elimination (after)
## CHECK: StaticFieldSet field_name:Main.field field_type:Bool
## CHECK: TypeConversion
.method public static setGetBooleanConst257()Z
    .registers 1
    const/16 v0, 0x101
    sput-boolean v0, LMain;->field:Z
    sget-boolean v0, LMain;->field:Z
    return v0
.end method

## CHECK-START: boolean BooleanTest.setGetBooleanConst511() load_store_elimination (before)
## CHECK: StaticFieldSet field_name:Main.field field_type:Bool
## CHECK: StaticFieldGet field_name:Main.field field_type:Bool

## CHECK-START: boolean BooleanTest.setGetBooleanConst511() load_store_elimination (after)
## CHECK-NOT: StaticFieldGet field_name:Main.field field_type:Bool

## CHECK-START: boolean BooleanTest.setGetBooleanConst511() load_store_elimination (after)
## CHECK: StaticFieldSet field_name:Main.field field_type:Bool
## CHECK: TypeConversion
.method public static setGetBooleanConst511()Z
    .registers 1
    const/16 v0, 0x1FF
    sput-boolean v0, LMain;->field:Z
    sget-boolean v0, LMain;->field:Z
    return v0
.end method

## CHECK-START: boolean BooleanTest.setGetBooleanConst512() load_store_elimination (before)
## CHECK: StaticFieldSet field_name:Main.field field_type:Bool
## CHECK: StaticFieldGet field_name:Main.field field_type:Bool

## CHECK-START: boolean BooleanTest.setGetBooleanConst512() load_store_elimination (after)
## CHECK-NOT: StaticFieldGet field_name:Main.field field_type:Bool

## CHECK-START: boolean BooleanTest.setGetBooleanConst512() load_store_elimination (after)
## CHECK: StaticFieldSet field_name:Main.field field_type:Bool
## CHECK: TypeConversion
.method public static setGetBooleanConst512()Z
    .registers 1
    const/16 v0, 0x200
    sput-boolean v0, LMain;->field:Z
    sget-boolean v0, LMain;->field:Z
    return v0
.end method

## CHECK-START: boolean BooleanTest.setGetBooleanConst513() load_store_elimination (before)
## CHECK: StaticFieldSet field_name:Main.field field_type:Bool
## CHECK: StaticFieldGet field_name:Main.field field_type:Bool

## CHECK-START: boolean BooleanTest.setGetBooleanConst513() load_store_elimination (after)
## CHECK-NOT: StaticFieldGet field_name:Main.field field_type:Bool

## CHECK-START: boolean BooleanTest.setGetBooleanConst513() load_store_elimination (after)
## CHECK: StaticFieldSet field_name:Main.field field_type:Bool
## CHECK: TypeConversion
.method public static setGetBooleanConst513()Z
    .registers 1
    const/16 v0, 0x201
    sput-boolean v0, LMain;->field:Z
    sget-boolean v0, LMain;->field:Z
    return v0
.end method

## CHECK-START: boolean BooleanTest.setGetBoolean(int) load_store_elimination (before)
## CHECK: StaticFieldSet field_name:Main.field field_type:Bool
## CHECK: StaticFieldGet field_name:Main.field field_type:Bool

## CHECK-START: boolean BooleanTest.setGetBoolean(int) load_store_elimination (after)
## CHECK-NOT: StaticFieldGet field_name:Main.field field_type:Bool

## CHECK-START: boolean BooleanTest.setGetBoolean(int) load_store_elimination (after)
## CHECK: StaticFieldSet field_name:Main.field field_type:Bool
## CHECK: TypeConversion
.method public static setGetBoolean(I)Z
    .registers 1
    sput-boolean p0, LMain;->field:Z
    sget-boolean v0, LMain;->field:Z
    return v0
.end method

# Implementation of `p0 ? 1 : 0`.
.method public static ternaryOperator(Z)I
    .registers 1
    if-nez p0, :LNotZero
    const/4 v0, 0x0
    return v0
:LNotZero
    const/4 v0, 0x1
    return v0
.end method

# Proxy method that redirects the int parameter as boolean in smali.
.method public static ternaryOperatorProxy(I)I
    .registers 1
    invoke-static {p0}, LBooleanTest;->ternaryOperator(Z)I
    move-result v0
    return v0
.end method

# Returns 1 if `p0` is equal to itself after a store+load in a boolean.
.method public static equalToItself(Z)Z
    .registers 2
    sput-boolean p0, LMain;->field:Z
    sget-boolean v0, LMain;->field:Z
    if-eq p0, v0, :LEqual
    const/4 v0, 0x0
    return v0
:LEqual
    const/4 v0, 0x1
    return v0
.end method

# Proxy method that redirects the int parameter as boolean in smali.
.method public static equalToItselfProxy(I)Z
    .registers 1
    invoke-static {p0}, LBooleanTest;->equalToItself(Z)Z
    move-result v0
    return v0
.end method

# Returns 1 if `p0` is equal to `p1`
.method public static equalBooleans(ZZ)Z
    .registers 2
    if-eq p0, p1, :LEqual
    const/4 v0, 0x0
    return v0
:LEqual
    const/4 v0, 0x1
    return v0
.end method

# Proxy method that redirects the int parameter as boolean in smali.
.method public static equalBooleansProxy(II)Z
    .registers 2
    invoke-static {p0, p1}, LBooleanTest;->equalBooleans(ZZ)Z
    move-result v0
    return v0
.end method