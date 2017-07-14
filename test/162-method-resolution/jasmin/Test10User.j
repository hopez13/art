; Copyright (C) 2017 The Android Open Source Project
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

.class public pkg1/Test10User
.super java/lang/Object

.method public static test()V
    .limit stack 2
    .limit locals 0
    new pkg1/Test10MostDerived
    dup
    invokespecial pkg1/Test10MostDerived.<init>()V
    invokevirtual pkg1/Test10MostDerived.foo()V
    return
.end method
