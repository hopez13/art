# Copyright (C) 2021 The Android Open Source Project
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

.class public LMain;

.super Ljava/lang/Object;

# Just do simple sanity check that we remove the instance-of. Well formedness
# checks will be done in gtests.
## CHECK-START: int Main.$noinline$test(boolean) load_store_elimination (before)
## CHECK-DAG: InstanceOf
#
## CHECK-START: int Main.$noinline$test(boolean) load_store_elimination (after)
## CHECK-NOT: InstanceOf

# public static int $noinline$test(boolean escape) {
#   Foo f = new Foo();
#   f.intField = 7
#   if (escape) {
#     if (f instanceof Bar) {
#       $noinline$escape(f);
#     }
#   }
#   return f.intField;
# }
.method public static $noinline$test(Z)I
  .registers 10
  const v1, 7
  const v2, 0
  new-instance v0, LFoo;
  invoke-direct {v0}, LFoo;-><init>()V
  iput v1, v0, LFoo;->intField:I
  if-eq p0, v2, :finish
    # NB Baz doesn't exist
    instance-of v3, v0, LBaz;
    if-eq v3, v2, :finish
      invoke-static {v0}, LMain;->$noinline$escape(Ljava/lang/Object;)V
:finish
  iget v0, v0, LFoo;->intField:I
  return v0
.end method

.method public static $noinline$escape(Ljava/lang/Object;)V
  .registers 1
  return-void
.end method

.method public static main([Ljava/lang/String;)V
  .registers 5
  const v0, 0
  const v1, 7
  const-string v3, "FAIL! GOT "
  sget-object v4, Ljava/lang/System;->out:Ljava/io/PrintStream;

  invoke-static {v0}, LMain;->$noinline$test(Z)I
  move-result v2
  if-eq v2, v1, :continue1
  invoke-virtual {v4, v3}, Ljava/io/PrintStream;->print(Ljava/lang/Object;)V
  invoke-virtual {v4, v2}, Ljava/io/PrintStream;->println(I)V

  :continue1

  # const v0, 1
  # invoke-static {v0}, LMain;->$noinline$test(Z)I
  # move-result v2
  # if-eq v2, v1, :continue2
  # invoke-virtual {v4, v3}, Ljava/io/PrintStream;->print(Ljava/lang/Object;)V
  # invoke-virtual {v4, v2}, Ljava/io/PrintStream;->println(I)V

  # :continue2
  return-void
.end method