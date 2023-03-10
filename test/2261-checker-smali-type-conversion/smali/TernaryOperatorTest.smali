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

.class public LTypeConversion;

.super Ljava/lang/Object;

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
    invoke-static {p0}, LTypeConversion;->ternaryOperator(Z)I
    move-result v0
    return v0
.end method
