# Copyright 2016 The Android Open Source Project
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

.class LReturnInt;
.super Ljava/lang/Object;
.implements Ljava/lang/Runnable;

.field private mh:Ljava/lang/invoke/MethodHandle;

.method public constructor <init>()V
.registers 10
    invoke-direct {p0}, Ljava/lang/Object;-><init>()V
     # v1 = MethodHandles.lookup()
    invoke-static {}, Ljava/lang/invoke/MethodHandles;->lookup()Ljava/lang/invoke/MethodHandles$Lookup;
    move-result-object v1

    # v2 = int.class
    sget-object v2, Ljava/lang/Integer;->TYPE:Ljava/lang/Class;
    # v3 = MethodType.methodType(/* rtype= */ v2)
    invoke-static {v2}, Ljava/lang/invoke/MethodType;->methodType(Ljava/lang/Class;)Ljava/lang/invoke/MethodType;
    move-result-object v3

    const-string v4, "returnInt"

    # v5 = this.class
    invoke-virtual {p0}, Ljava/lang/Object;->getClass()Ljava/lang/Class;
    move-result-object v5

    # v6 = findVirtual
    invoke-virtual {v1, v5, v4, v3}, Ljava/lang/invoke/MethodHandles$Lookup;->findVirtual(Ljava/lang/Class;Ljava/lang/String;Ljava/lang/invoke/MethodType;)Ljava/lang/invoke/MethodHandle;
    move-result-object v6

    iput-object v6, p0, LReturnInt;->mh:Ljava/lang/invoke/MethodHandle;

    return-void
.end method

.annotation system Ldalvik/annotation/optimization/NeverInline;
.end annotation

.method public returnInt()I
.registers 1
    const v0, 0x2A
    return v0
.end method

.method public static returnIntInvokeBasic(Ljava/lang/invoke/MethodHandle;LReturnInt;)I
.registers 4
    const-method-type v0, (LReturnInt;)I
    invoke-static {p0, v0}, Ljava/lang/invoke/MethodHandle;->checkExactType(Ljava/lang/invoke/MethodHandle;Ljava/lang/invoke/MethodType;)V
    invoke-polymorphic {p0, p1}, Ljava/lang/invoke/MethodHandle;->invokeBasic([Ljava/lang/Object;)Ljava/lang/Object;, (LReturnInt;)I
    move-result v1
    return v1
.end method


.method public static returnIntInvokeBasic(Ljava/lang/invoke/MethodHandle;LReturnInt;Ljava/lang/invoke/MethodType;)I
.registers 4
    # const-method-type v0, (LReturnInt;)I
    invoke-static {p0, p2}, Ljava/lang/invoke/MethodHandle;->checkExactType(Ljava/lang/invoke/MethodHandle;Ljava/lang/invoke/MethodType;)V
    invoke-polymorphic {p0, p1}, Ljava/lang/invoke/MethodHandle;->invokeBasic([Ljava/lang/Object;)Ljava/lang/Object;, (LReturnInt;)I
    move-result v1
    return v1
.end method

.method public static returnIntInvokeExact(Ljava/lang/invoke/MethodHandle;LReturnInt;)I
.registers 3
    invoke-polymorphic {p0, p1}, Ljava/lang/invoke/MethodHandle;->invokeExact([Ljava/lang/Object;)Ljava/lang/Object;, (LReturnInt;)I
    move-result v0
    return v0
.end method

.method public run()V
.registers 20
    # v0 = new Main()
    new-instance v0, LReturnInt;
    invoke-direct {v0}, LReturnInt;-><init>()V

    iget-object v1, v0, LReturnInt;->mh:Ljava/lang/invoke/MethodHandle;

    invoke-static {}, Ljava/lang/System;->nanoTime()J
    move-result-wide v2

    const-method-type v11, (LReturnInt;)I

    const/4 v4, 0x0
    # v5 = 1000
    const/16 v5, 0x3e8
    :start_loop
    if-ge v4, v5, :exit_loop
    # 50ns
    invoke-static {v1, v0, v11}, LReturnInt;->returnIntInvokeBasic(Ljava/lang/invoke/MethodHandle;LReturnInt;Ljava/lang/invoke/MethodType;)I
    # 3016ns
    # invoke-static {v1, v0}, LReturnInt;->returnIntInvokeBasic(Ljava/lang/invoke/MethodHandle;LReturnInt;)I
    # 7ns
    # invoke-virtual {v0}, LReturnInt;->returnInt()I
    # 6000ns
    # invoke-static {v1, v0}, LReturnInt;->returnIntInvokeExact(Ljava/lang/invoke/MethodHandle;LReturnInt;)I
    add-int/lit8 v4, v4, 1
    goto :start_loop
    :exit_loop
    invoke-static {}, Ljava/lang/System;->nanoTime()J
    move-result-wide v6

    sub-long v8, v6, v2

    sget-object v10, Ljava/lang/System;->out:Ljava/io/PrintStream;
    invoke-virtual {v10,v8,v9}, Ljava/io/PrintStream;->println(J)V

    return-void
.end method

.method public static main([Ljava/lang/String;)V
.registers 20
    return-void
.end method
