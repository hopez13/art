#
# Copyright (C) 2016 The Android Open Source Project
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

.class public LMain;
.super Ljava/lang/Object;

.method public constructor<init>()V
  .registers 1
  invoke-direct {v0}, Ljava/lang/Object;-><init>()V
  return-void
.end method

.method public static main([Ljava/lang/String;)V
  .registers 1
  const-string/jumbo v0, "A"
  invoke-static {v0}, LMain;->test(Ljava/lang/String;)V
  const-string/jumbo v0, "B"
  invoke-static {v0}, LMain;->test(Ljava/lang/String;)V
  const-string/jumbo v0, "C"
  invoke-static {v0}, LMain;->test(Ljava/lang/String;)V
  const-string/jumbo v0, "D"
  invoke-static {v0}, LMain;->test(Ljava/lang/String;)V
  const-string/jumbo v0, "E"
  invoke-static {v0}, LMain;->test(Ljava/lang/String;)V
  const-string/jumbo v0, "F"
  invoke-static {v0}, LMain;->test(Ljava/lang/String;)V
  return-void
.end method

.method public static test(Ljava/lang/String;)V
  .registers 6
 :try_start_1
  invoke-static {v5}, Ljava/lang/Class;->forName(Ljava/lang/String;)Ljava/lang/Class;
  move-result-object v0
  invoke-virtual {v0}, Ljava/lang/Class;->newInstance()Ljava/lang/Object;
 :try_end_1
  .catch Ljava/lang/VerifyError; {:try_start_1 .. :try_end_1} :catch_verifier
  return-void
 :catch_verifier
  move-exception v3
  invoke-virtual {v3}, Ljava/lang/Exception;->toString()Ljava/lang/String;
  move-result-object v3
  sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
  invoke-virtual {v2, v3}, Ljava/io/PrintStream;->println(Ljava/lang/String;)V
  return-void
.end method

# A test method called "invoke", but on a class other than MethodHandle.
.method public invoke([Ljava/lang/Object;)Ljava/lang/Object;
  .registers 2
  const/4 v0, 0
  aget-object v0, p0, v0
  return-object v0
.end method

# A test method called "invokeExact" that is native varargs, but is on a class
# other than MethodHandle.
.method public native varargs invokeExact([Ljava/lang/Object;)Ljava/lang/Object;
.end method