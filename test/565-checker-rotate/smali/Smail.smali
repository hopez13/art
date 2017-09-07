.class public LSmali;
.super Ljava/lang/Object;

##  CHECK-START: int Smali.rotateLeftBoolean(boolean, int) intrinsics_recognition (after)
##  CHECK-DAG:     <<Method:[ij]\d+>> CurrentMethod
##  CHECK:         <<ArgVal:z\d+>>  ParameterValue
##  CHECK:         <<ArgDist:i\d+>> ParameterValue
##  CHECK-DAG:     <<Zero:i\d+>>    IntConstant 0
##  CHECK-DAG:     <<One:i\d+>>     IntConstant 1
##  CHECK-DAG:     <<Val:i\d+>>     Phi [<<One>>,<<Zero>>]
##  CHECK-DAG:     <<Result:i\d+>>  InvokeStaticOrDirect [<<Val>>,<<ArgDist>>,<<Method>>] intrinsic:IntegerRotateLeft
##  CHECK-DAG:                      Return [<<Result>>]

##  CHECK-START: int Smali.rotateLeftBoolean(boolean, int) instruction_simplifier (after)
##  CHECK:         <<ArgVal:z\d+>>  ParameterValue
##  CHECK:         <<ArgDist:i\d+>> ParameterValue
##  CHECK-DAG:     <<Zero:i\d+>>    IntConstant 0
##  CHECK-DAG:     <<One:i\d+>>     IntConstant 1
##  CHECK-DAG:     <<Val:i\d+>>     Phi [<<One>>,<<Zero>>]
##  CHECK-DAG:     <<NegDist:i\d+>> Neg [<<ArgDist>>]
##  CHECK-DAG:     <<Result:i\d+>>  Ror [<<Val>>,<<NegDist>>]
##  CHECK-DAG:                      Return [<<Result>>]

##  CHECK-START: int Smali.rotateLeftBoolean(boolean, int) instruction_simplifier (after)
##  CHECK-NOT:                      InvokeStaticOrDirect

##  CHECK-START: int Smali.rotateLeftBoolean(boolean, int) select_generator (after)
##  CHECK:         <<ArgVal:z\d+>>  ParameterValue
##  CHECK:         <<ArgDist:i\d+>> ParameterValue
##  CHECK-DAG:     <<Zero:i\d+>>    IntConstant 0
##  CHECK-DAG:     <<One:i\d+>>     IntConstant 1
##  CHECK-DAG:     <<SelVal:i\d+>>  Select [<<Zero>>,<<One>>,<<ArgVal>>]
##  CHECK-DAG:     <<NegDist:i\d+>> Neg [<<ArgDist>>]
##  CHECK-DAG:     <<Result:i\d+>>  Ror [<<SelVal>>,<<NegDist>>]
##  CHECK-DAG:                      Return [<<Result>>]

##  CHECK-START: int Smali.rotateLeftBoolean(boolean, int) select_generator (after)
##  CHECK-NOT:                      Phi

##  CHECK-START: int Smali.rotateLeftBoolean(boolean, int) instruction_simplifier$after_bce (after)
##  CHECK:         <<ArgVal:z\d+>>  ParameterValue
##  CHECK:         <<ArgDist:i\d+>> ParameterValue
##  CHECK-DAG:     <<NegDist:i\d+>> Neg [<<ArgDist>>]
##  CHECK-DAG:     <<Result:i\d+>>  Ror [<<ArgVal>>,<<NegDist>>]
##  CHECK-DAG:                      Return [<<Result>>]

##  CHECK-START: int Smali.rotateLeftBoolean(boolean, int) instruction_simplifier$after_bce (after)
##  CHECK-NOT:                      Select

#   Note: A dexer like D8 can also recognize value ? : 0 -> value
#         so we are writing this in Smali.
.method public static rotateLeftBoolean(ZI)I
    # return Integer.rotateLeft(value ? 1 : 0, distance);

    .registers 3
    if-eqz p0, :cond_8

    const/4 v0, 0x1

    :goto_3
    invoke-static {v0, p1}, Ljava/lang/Integer;->rotateLeft(II)I

    move-result v0

    return v0

    :cond_8
    const/4 v0, 0x0

    goto :goto_3
.end method

##  CHECK-START: int Smali.rotateRightBoolean(boolean, int) intrinsics_recognition (after)
##  CHECK-DAG:     <<Method:[ij]\d+>> CurrentMethod
##  CHECK:         <<ArgVal:z\d+>>  ParameterValue
##  CHECK:         <<ArgDist:i\d+>> ParameterValue
##  CHECK-DAG:     <<Zero:i\d+>>    IntConstant 0
##  CHECK-DAG:     <<One:i\d+>>     IntConstant 1
##  CHECK-DAG:     <<Val:i\d+>>     Phi [<<One>>,<<Zero>>]
##  CHECK-DAG:     <<Result:i\d+>>  InvokeStaticOrDirect [<<Val>>,<<ArgDist>>,<<Method>>] intrinsic:IntegerRotateRight
##  CHECK-DAG:                     Return [<<Result>>]

##  CHECK-START: int Smali.rotateRightBoolean(boolean, int) instruction_simplifier (after)
##  CHECK:         <<ArgVal:z\d+>>  ParameterValue
##  CHECK:         <<ArgDist:i\d+>> ParameterValue
##  CHECK-DAG:     <<Zero:i\d+>>    IntConstant 0
##  CHECK-DAG:     <<One:i\d+>>     IntConstant 1
##  CHECK-DAG:     <<Val:i\d+>>     Phi [<<One>>,<<Zero>>]
##  CHECK-DAG:     <<Result:i\d+>>  Ror [<<Val>>,<<ArgDist>>]
##  CHECK-DAG:                      Return [<<Result>>]

##  CHECK-START: int Smali.rotateRightBoolean(boolean, int) instruction_simplifier (after)
##  CHECK-NOT:                      InvokeStaticOrDirect

##  CHECK-START: int Smali.rotateRightBoolean(boolean, int) select_generator (after)
##  CHECK:         <<ArgVal:z\d+>>  ParameterValue
##  CHECK:         <<ArgDist:i\d+>> ParameterValue
##  CHECK-DAG:     <<Zero:i\d+>>    IntConstant 0
##  CHECK-DAG:     <<One:i\d+>>     IntConstant 1
##  CHECK-DAG:     <<SelVal:i\d+>>  Select [<<Zero>>,<<One>>,<<ArgVal>>]
##  CHECK-DAG:     <<Result:i\d+>>  Ror [<<SelVal>>,<<ArgDist>>]
##  CHECK-DAG:                      Return [<<Result>>]

##  CHECK-START: int Smali.rotateRightBoolean(boolean, int) select_generator (after)
##  CHECK-NOT:                     Phi

##  CHECK-START: int Smali.rotateRightBoolean(boolean, int) instruction_simplifier$after_bce (after)
##  CHECK:         <<ArgVal:z\d+>>  ParameterValue
##  CHECK:         <<ArgDist:i\d+>> ParameterValue
##  CHECK-DAG:     <<Result:i\d+>>  Ror [<<ArgVal>>,<<ArgDist>>]
##  CHECK-DAG:                      Return [<<Result>>]

##  CHECK-START: int Smali.rotateRightBoolean(boolean, int) instruction_simplifier$after_bce (after)
##  CHECK-NOT:                     Select

#   Note: A dexer like D8 can also recognize value ? : 0 -> value
#         so we are writing this in Smali.
.method public static rotateRightBoolean(ZI)I
    # return Integer.rotateRight(value ? 1 : 0, distance);
    .registers 3
    if-eqz p0, :cond_8

    const/4 v0, 0x1

    :goto_3
    invoke-static {v0, p1}, Ljava/lang/Integer;->rotateRight(II)I

    move-result v0

    return v0

    :cond_8
    const/4 v0, 0x0

    goto :goto_3
.end method
