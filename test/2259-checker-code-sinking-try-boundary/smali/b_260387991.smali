#
# Copyright (C) 2022 The Android Open Source Project
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
.class public LB252803217;

.super Ljava/lang/Object;

# Check that the add doesn't get "sunk" into the main body

## CHECK-START: int B252803217.testInfiniteLoopUnlessMethodReturns() code_sinking (before)
## CHECK:      Nop
## CHECK-NEXT: Add
## CHECK-NEXT: Goto

## CHECK-START: int B252803217.testInfiniteLoopUnlessMethodReturns() code_sinking (after)
## CHECK:      Nop
## CHECK-NEXT: Add
## CHECK-NEXT: Goto
.method public static testInfiniteLoopUnlessMethodReturns()I
   .registers 4
    const/4 v0, 0x0
    const/4 v1, 0x1
    :try_start
    # This method doesn't do anything but keeps the try alive, and keeps v0 from being unused.
    invoke-static {v0}, LB252803217;->$noinline$returnZeroIgnoreParameter(I)I
    move-result v0
    return v0
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
