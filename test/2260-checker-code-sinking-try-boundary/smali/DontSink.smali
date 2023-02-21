
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

.class public LDontSink;
.super Ljava/lang/Object;

# Consistency check: only one add.
## CHECK-START: int DontSink.testDontSinkToReturnTryBoundary(int, int, boolean) code_sinking (before)
## CHECK:      Add
## CHECK-NOT:  Add

## CHECK-START: int DontSink.testDontSinkToReturnTryBoundary(int, int, boolean) code_sinking (before)
## CHECK:      Add
## CHECK-NEXT: If

## CHECK-START: int DontSink.testDontSinkToReturnTryBoundary(int, int, boolean) code_sinking (after)
## CHECK:      Add
## CHECK-NEXT: If
.method public static testDontSinkToReturnTryBoundary(IIZ)I
    .registers 5
    add-int v0, p0, p1

    if-eqz p2, :try_start
    const/4 v1, 0x1
    return v1

    :try_start
    # This method doesn't do anything but keeps the try alive, and keeps v0 from being unused.
    invoke-static {v0}, LDontSink;->$noinline$returnZeroIgnoreParameter(I)I
    move-result v0
    return v0
    :try_end
    .catchall {:try_start .. :try_end} :catch_all
    :catch_all
    return p0
.end method

.method public static $noinline$returnZeroIgnoreParameter(I)I
    .registers 1
    const/4 v0, 0x0
    return v0
.end method
