.class LMain;
.super Ljava/lang/Object;

.method public static getHandle()Ljava/lang/invoke/MethodHandle;
.registers 5

    const-string v1, "concat"
    invoke-virtual {v1}, Ljava/lang/Object;->getClass()Ljava/lang/Class;
    move-result-object v0
    const v2, 0
    invoke-static {v0, v0}, Ljava/lang/invoke/MethodType;->methodType(Ljava/lang/Class;Ljava/lang/Class;)Ljava/lang/invoke/MethodType;
    move-result-object v3
    invoke-static {v2, v0, v1, v3}, Ljava/lang/invoke/MethodHandleImpl;->lookupVirtual(Ljava/lang/Class;Ljava/lang/Class;Ljava/lang/String;Ljava/lang/invoke/MethodType;)Ljava/lang/invoke/MethodHandle;
    move-result-object v4
    return-object v4
.end method

.method public static main([Ljava/lang/String;)V
.registers 5

    invoke-static {}, Ljava/lang/invoke/MethodHandleImpl;->charsequence_charAt()Ljava/lang/invoke/MethodHandle;
    move-result-object v0
    const-string v1, "[Appendee]"
    const v2, 1
    invoke-polymorphic {v0, v1, v2}, Ljava/lang/invoke/MethodHandle;->invoke([Ljava/lang/Object;)Ljava/lang/Object;, (Ljava/lang/Object;S)C
    move-result v3
    sget-object v4, Ljava/lang/System;->out:Ljava/io/PrintStream;
    invoke-virtual {v4, v3}, Ljava/io/PrintStream;->println(C)V
    return-void
.end method
