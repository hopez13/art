#
# Copyright (C) 2023 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
.class public LB269927803;

.super Ljava/lang/Object;

# Note that we will check that this method compiles but it will
# loop infinitely, so we won't run it. Our graph builder won't
# generate code like this from java, but we can generate it with smali.

## CHECK-START: int B269927803.testInfiniteLoopCatchJumpsToTry(java.lang.Exception) code_sinking (before)
## CHECK-NOT: Add loop:none

## CHECK-START: int B269927803.testInfiniteLoopCatchJumpsToTry(java.lang.Exception) code_sinking (before)
## CHECK-DAG:   <<Const0:i\d+>> IntConstant 0
## CHECK-DAG:   <<Add:i\d+>> Add loop:<<Loop:B\d+>> outer_loop:none
## CHECK-DAG:   Phi [<<Const0>>,<<Add>>] loop:<<Loop>> outer_loop:none

## CHECK-START: int B269927803.testInfiniteLoopCatchJumpsToTry(java.lang.Exception) code_sinking (after)
## CHECK-NOT: Add loop:none

## CHECK-START: int B269927803.testInfiniteLoopCatchJumpsToTry(java.lang.Exception) code_sinking (after)
## CHECK-DAG:   <<Const0:i\d+>> IntConstant 0
## CHECK-DAG:   <<Add:i\d+>> Add loop:<<Loop:B\d+>> outer_loop:none
## CHECK-DAG:   Phi [<<Const0>>,<<Add>>] loop:<<Loop>> outer_loop:none

.method public static testInfiniteLoopCatchJumpsToTry(Ljava/lang/Exception;)I
   .registers 4
    const/4 v0, 0x0
    const/4 v1, 0x1
    :try_start
    # This method doesn't do anything but keeps the try alive, and keeps v0 from being unused.
    invoke-static {v0}, LB269927803;->$noinline$returnZeroIgnoreParameter(I)I
    throw p0
    :try_end
    .catchall {:try_start .. :try_end} :catch_all

    :catch_all
    add-int v0, v0, v1
    goto :try_start
.end method

.method public static $noinline$returnZeroIgnoreParameter(I)I
   .registers 1
    const/4 v0, 0x0
    return v0
.end method
