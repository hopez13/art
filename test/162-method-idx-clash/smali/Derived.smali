#
# Copyright (C) 2017 The Android Open Source Project
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
#

.class public LDerived;
.super LBase;

.method public constructor <init>()V
    .registers 1
    invoke-direct {v0}, LBase;-><init>()V
    return-void
.end method

.method public static test()V
    .registers 1
    new-instance v0, LDerived;
    invoke-direct {v0}, LDerived;-><init>()V
    invoke-direct {v0}, LDerived;->foo()V
    return-void
.end method

.method private foo()V
    .registers 3
    sget-object v0, Ljava/lang/System;->out:Ljava/io/PrintStream;
    const-string v1, "Derived.foo()"
    invoke-virtual {v0, v1}, Ljava/io/PrintStream;->println(Ljava/lang/Object;)V
    return-void
.end method
