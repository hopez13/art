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

.class public LPassNullArray;

.super Ljava/lang/Object;

.method public static methodF()F
   .registers 2
   const/4 v0, 0
   const/4 v1, 0
   aget v0, v0, v1
   return v0
.end method

.method public static methodI()I
   .registers 2
   const/4 v0, 0
   const/4 v1, 0
   aget v0, v0, v1
   return v0
.end method

.method public static methodB()I
   .registers 2
   const/4 v0, 0
   const/4 v1, 0
   aget-byte v0, v0, v1
   return v0
.end method

.method public static methodC()I
   .registers 2
   const/4 v0, 0
   const/4 v1, 0
   aget-char v0, v0, v1
   return v0
.end method

.method public static methodS()I
   .registers 2
   const/4 v0, 0
   const/4 v1, 0
   aget-short v0, v0, v1
   return v0
.end method

.method public static methodZ()I
   .registers 2
   const/4 v0, 0
   const/4 v1, 0
   aget-boolean v0, v0, v1
   return v0
.end method
