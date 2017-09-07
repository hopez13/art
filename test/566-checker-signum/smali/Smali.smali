.class public LSmali;
.super Ljava/lang/Object;

##  CHECK-START: int Smali.signBoolean(boolean) intrinsics_recognition (after)
##  CHECK-DAG:     <<Method:[ij]\d+>> CurrentMethod
##  CHECK-DAG:     <<Zero:i\d+>>   IntConstant 0
##  CHECK-DAG:     <<One:i\d+>>    IntConstant 1
##  CHECK-DAG:     <<Phi:i\d+>>    Phi [<<One>>,<<Zero>>]
##  CHECK-DAG:     <<Result:i\d+>> InvokeStaticOrDirect [<<Phi>>,<<Method>>] intrinsic:IntegerSignum
##  CHECK-DAG:                     Return [<<Result>>]

##  CHECK-START: int Smali.signBoolean(boolean) instruction_simplifier (after)
##  CHECK-DAG:     <<Zero:i\d+>>   IntConstant 0
##  CHECK-DAG:     <<One:i\d+>>    IntConstant 1
##  CHECK-DAG:     <<Phi:i\d+>>    Phi [<<One>>,<<Zero>>]
##  CHECK-DAG:     <<Result:i\d+>> Compare [<<Phi>>,<<Zero>>]
##  CHECK-DAG:                     Return [<<Result>>]

##  CHECK-START: int Smali.signBoolean(boolean) instruction_simplifier (after)
##  CHECK-NOT:                     InvokeStaticOrDirect

##  CHECK-START: int Smali.signBoolean(boolean) select_generator (after)
##  CHECK-DAG:     <<Arg:z\d+>>    ParameterValue
##  CHECK-DAG:     <<Zero:i\d+>>   IntConstant 0
##  CHECK-DAG:     <<One:i\d+>>    IntConstant 1
##  CHECK-DAG:     <<Sel:i\d+>>    Select [<<Zero>>,<<One>>,<<Arg>>]
##  CHECK-DAG:     <<Result:i\d+>> Compare [<<Sel>>,<<Zero>>]
##  CHECK-DAG:                     Return [<<Result>>]

##  CHECK-START: int Smali.signBoolean(boolean) select_generator (after)
##  CHECK-NOT:                     Phi

##  CHECK-START: int Smali.signBoolean(boolean) instruction_simplifier$after_bce (after)
##  CHECK-DAG:     <<Arg:z\d+>>    ParameterValue
##  CHECK-DAG:     <<Zero:i\d+>>   IntConstant 0
##  CHECK-DAG:     <<Result:i\d+>> Compare [<<Arg>>,<<Zero>>]
##  CHECK-DAG:                     Return [<<Result>>]

##  CHECK-START: int Smali.signBoolean(boolean) instruction_simplifier$after_bce (after)
##  CHECK-NOT:                     Select

#   Note: Dexer such as D8 can perform valid LSE as well so we must write these
#         tests in Smali.
.method public static signBoolean(Z)I
    # return Integer.signum(x ? 1 : 0);

    .registers 2
    if-eqz p0, :cond_8

    const/4 v0, 0x1

    :goto_3
    invoke-static {v0}, Ljava/lang/Integer;->signum(I)I

    move-result v0

    return v0

    :cond_8
    const/4 v0, 0x0

    goto :goto_3
.end method
