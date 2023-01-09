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
.class public LB264903868;

.super Ljava/lang/Object;

# We were crashing on the GraphChecker when inlining `$inline$crash`
# into `caller` due to a mis-optimization regaring Rem.

.method public static $inline$crash(I)I
   .registers 2
    const/16 v0, 0x32 # 50
    rem-int/lit16 v1, p0, 0x32 # Rem(p0, 50)
    sub-int v1, v0, v1 # Sub(50, Rem)
    div-int/lit16 v1, v1, 0x32 # Div(Sub, 50)
    rem-int/lit8 v1, v1, 0x2 # Rem(Div, 2)
    return v1
.end method

.method public static caller()I
   .registers 1
    const/16 v0, 0x32 # 50
    invoke-static {v0}, LB264903868;->$inline$crash(I)I
    move-result v0
    return v0
.end method
