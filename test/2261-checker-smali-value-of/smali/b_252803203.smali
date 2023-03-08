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
.class public LB252803203;

.super Ljava/lang/Object;

## CHECK-START: java.lang.Boolean B252803203.crash(boolean) inliner (before)
## CHECK-DAG: <<Const0:i\d+>>     IntConstant 0
## CHECK-DAG: <<Const4:i\d+>>     IntConstant 4
## CHECK-DAG: <<Phi:i\d+>>        Phi [<<Const4>>,<<Const0>>]
## CHECK-DAG:                     InvokeStaticOrDirect [<<Phi>>] method_name:B252803203.booleanValueOf

## CHECK-START: java.lang.Boolean B252803203.crash(boolean) inliner (after)
## CHECK-DAG: <<Const0:i\d+>>     IntConstant 0
## CHECK-DAG: <<Const4:i\d+>>     IntConstant 4
## CHECK-DAG: <<Phi:i\d+>>        Phi [<<Const4>>,<<Const0>>]
## CHECK-DAG:                     If [<<Phi>>]

# DCE eliminates the Phi+If without crashing
## CHECK-START: java.lang.Boolean B252803203.crash(boolean) dead_code_elimination$after_inlining (before)
## CHECK-DAG: <<Const0:i\d+>>     IntConstant 0
## CHECK-DAG: <<Const4:i\d+>>     IntConstant 4
## CHECK-DAG: <<Phi:i\d+>>        Phi [<<Const4>>,<<Const0>>]
## CHECK-DAG:                     If [<<Phi>>]

## CHECK-START: java.lang.Boolean B252803203.crash(boolean) dead_code_elimination$after_inlining (after)
## CHECK-NOT: IntConstant
.method public static crash(Z)Ljava/lang/Boolean;
   .registers 3
   if-eqz p0, :LFalse
   const/4 v0, 0x4
   goto :LCall
:LFalse
   const/4 v0, 0x0
:LCall
   invoke-static {v0}, LB252803203;->booleanValueOf(Z)Ljava/lang/Boolean;
   move-result-object v0
   return-object v0
.end method

# Copy of java.lang.Boolean's ValueOf(boolean)
.method public static booleanValueOf(Z)Ljava/lang/Boolean;
   .registers 1
   if-eqz p0, :LFalse
   sget-object v0, Ljava/lang/Boolean;->TRUE:Ljava/lang/Boolean;
   return-object v0
:LFalse
   sget-object v0, Ljava/lang/Boolean;->FALSE:Ljava/lang/Boolean;
   return-object v0
.end method
