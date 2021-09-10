# Copyright (C) 2021 The Android Open Source Project
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

.class public LRemoveSuspendCheck;
.super Ljava/lang/Object;
.field public str:Ljava/lang/String;
.field public arr:[Ljava/lang/String;

.method public constructor <init>()V
.registers 1
    invoke-direct {p0}, Ljava/lang/Object;-><init>()V
    return-void
.end method

# We should be able to eliminate the SuspendCheck when reading a field.
## CHECK-START: java.lang.String RemoveSuspendCheck.fieldLeaf(RemoveSuspendCheck) register (after)
## CHECK-NOT: SuspendCheck
## CHECK: InstanceFieldGet
.method public fieldLeaf(LRemoveSuspendCheck;)Ljava/lang/String;
.registers 2
    iget-object v0, p0, LRemoveSuspendCheck;->str:Ljava/lang/String;
    return-object v0
.end method

# We should be able to eliminate the SuspendCheck when reading an item from an array.
## CHECK-START: java.lang.String RemoveSuspendCheck.arrayGetLeaf(RemoveSuspendCheck) register (after)
## CHECK-NOT: SuspendCheck
## CHECK: ArrayGet
.method public arrayGetLeaf(LRemoveSuspendCheck;)Ljava/lang/String;
.registers 2
    iget-object v0, p0, LRemoveSuspendCheck;->arr:[Ljava/lang/String;
    const v1, 0x0
    aget-object v1, v0, v1
    return-object v1
.end method

.method public static removeSuspendCheckFromGets()V
    .registers 2
    new-instance v0, LRemoveSuspendCheck;
    invoke-direct {v0}, LRemoveSuspendCheck;-><init>()V
    invoke-virtual {v0, v0}, LRemoveSuspendCheck;->fieldLeaf(LRemoveSuspendCheck;)Ljava/lang/String;

    const-string v1, "test"
    filled-new-array {v1, v1, v1}, [Ljava/lang/String;
    move-result-object v1
    iput-object v1, v0, LRemoveSuspendCheck;->arr:[Ljava/lang/String;
    invoke-virtual {v0, v0}, LRemoveSuspendCheck;->arrayGetLeaf(LRemoveSuspendCheck;)Ljava/lang/String;
    return-void
.end method

# We should be able to eliminate the SuspendCheck when loading a class.
## CHECK-START: java.lang.Class RemoveSuspendCheck.removeSuspendCheckFromLoadClass() register (after)
## CHECK-NOT: SuspendCheck
## CHECK: LoadClass
.method public static removeSuspendCheckFromLoadClass()Ljava/lang/Class;
    .registers 1
    const-class v0, LRemoveSuspendCheck;
    return-object v0
.end method