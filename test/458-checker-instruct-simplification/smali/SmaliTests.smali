# Copyright (C) 2016 The Android Open Source Project
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

.class public LSmaliTests;
.super Ljava/lang/Object;

## CHECK-START: int SmaliTests.EqualTrueRhs(boolean) instruction_simplifier (before)
## CHECK-DAG:     <<Arg:z\d+>>      ParameterValue
## CHECK-DAG:     <<Const1:i\d+>>   IntConstant 1
## CHECK-DAG:     <<Cond:z\d+>>     Equal [<<Arg>>,<<Const1>>]
## CHECK-DAG:                       If [<<Cond>>]

## CHECK-START: int SmaliTests.EqualTrueRhs(boolean) instruction_simplifier (after)
## CHECK-DAG:     <<Arg:z\d+>>      ParameterValue
## CHECK-DAG:                       If [<<Arg>>]

.method public static EqualTrueRhs(Z)I
  .registers 3

  const v0, 0x1
  const v1, 0x5
  if-eq p0, v0, :return
  const v1, 0x3
  :return
  return v1

.end method

## CHECK-START: int SmaliTests.EqualTrueLhs(boolean) instruction_simplifier (before)
## CHECK-DAG:     <<Arg:z\d+>>      ParameterValue
## CHECK-DAG:     <<Const1:i\d+>>   IntConstant 1
## CHECK-DAG:     <<Cond:z\d+>>     Equal [<<Const1>>,<<Arg>>]
## CHECK-DAG:                       If [<<Cond>>]

## CHECK-START: int SmaliTests.EqualTrueLhs(boolean) instruction_simplifier (after)
## CHECK-DAG:     <<Arg:z\d+>>      ParameterValue
## CHECK-DAG:                       If [<<Arg>>]

.method public static EqualTrueLhs(Z)I
  .registers 3

  const v0, 0x1
  const v1, 0x5
  if-eq v0, p0, :return
  const v1, 0x3
  :return
  return v1

.end method

## CHECK-START: int SmaliTests.EqualFalseRhs(boolean) instruction_simplifier (before)
## CHECK-DAG:     <<Arg:z\d+>>      ParameterValue
## CHECK-DAG:     <<Const0:i\d+>>   IntConstant 0
## CHECK-DAG:     <<Cond:z\d+>>     Equal [<<Arg>>,<<Const0>>]
## CHECK-DAG:                       If [<<Cond>>]

## CHECK-START: int SmaliTests.EqualFalseRhs(boolean) instruction_simplifier (after)
## CHECK-DAG:     <<Arg:z\d+>>      ParameterValue
## CHECK-DAG:                       If [<<Arg>>]

.method public static EqualFalseRhs(Z)I
  .registers 3

  const v0, 0x0
  const v1, 0x3
  if-eq p0, v0, :return
  const v1, 0x5
  :return
  return v1

.end method

## CHECK-START: int SmaliTests.EqualFalseLhs(boolean) instruction_simplifier (before)
## CHECK-DAG:     <<Arg:z\d+>>      ParameterValue
## CHECK-DAG:     <<Const0:i\d+>>   IntConstant 0
## CHECK-DAG:     <<Cond:z\d+>>     Equal [<<Const0>>,<<Arg>>]
## CHECK-DAG:                       If [<<Cond>>]

## CHECK-START: int SmaliTests.EqualFalseLhs(boolean) instruction_simplifier (after)
## CHECK-DAG:     <<Arg:z\d+>>      ParameterValue
## CHECK-DAG:                       If [<<Arg>>]

.method public static EqualFalseLhs(Z)I
  .registers 3

  const v0, 0x0
  const v1, 0x3
  if-eq v0, p0, :return
  const v1, 0x5
  :return
  return v1

.end method

## CHECK-START: int SmaliTests.NotEqualTrueRhs(boolean) instruction_simplifier (before)
## CHECK-DAG:     <<Arg:z\d+>>      ParameterValue
## CHECK-DAG:     <<Const1:i\d+>>   IntConstant 1
## CHECK-DAG:     <<Cond:z\d+>>     NotEqual [<<Arg>>,<<Const1>>]
## CHECK-DAG:                       If [<<Cond>>]

## CHECK-START: int SmaliTests.NotEqualTrueRhs(boolean) instruction_simplifier (after)
## CHECK-DAG:     <<Arg:z\d+>>      ParameterValue
## CHECK-DAG:                       If [<<Arg>>]

.method public static NotEqualTrueRhs(Z)I
  .registers 3

  const v0, 0x1
  const v1, 0x3
  if-ne p0, v0, :return
  const v1, 0x5
  :return
  return v1

.end method

## CHECK-START: int SmaliTests.NotEqualTrueLhs(boolean) instruction_simplifier (before)
## CHECK-DAG:     <<Arg:z\d+>>      ParameterValue
## CHECK-DAG:     <<Const1:i\d+>>   IntConstant 1
## CHECK-DAG:     <<Cond:z\d+>>     NotEqual [<<Const1>>,<<Arg>>]
## CHECK-DAG:                       If [<<Cond>>]

## CHECK-START: int SmaliTests.NotEqualTrueLhs(boolean) instruction_simplifier (after)
## CHECK-DAG:     <<Arg:z\d+>>      ParameterValue
## CHECK-DAG:                       If [<<Arg>>]

.method public static NotEqualTrueLhs(Z)I
  .registers 3

  const v0, 0x1
  const v1, 0x3
  if-ne v0, p0, :return
  const v1, 0x5
  :return
  return v1

.end method

## CHECK-START: int SmaliTests.NotEqualFalseRhs(boolean) instruction_simplifier (before)
## CHECK-DAG:     <<Arg:z\d+>>      ParameterValue
## CHECK-DAG:     <<Const0:i\d+>>   IntConstant 0
## CHECK-DAG:     <<Cond:z\d+>>     NotEqual [<<Arg>>,<<Const0>>]
## CHECK-DAG:                       If [<<Cond>>]

## CHECK-START: int SmaliTests.NotEqualFalseRhs(boolean) instruction_simplifier (after)
## CHECK-DAG:     <<Arg:z\d+>>      ParameterValue
## CHECK-DAG:                       If [<<Arg>>]

.method public static NotEqualFalseRhs(Z)I
  .registers 3

  const v0, 0x0
  const v1, 0x5
  if-ne p0, v0, :return
  const v1, 0x3
  :return
  return v1

.end method

## CHECK-START: int SmaliTests.NotEqualFalseLhs(boolean) instruction_simplifier (before)
## CHECK-DAG:     <<Arg:z\d+>>      ParameterValue
## CHECK-DAG:     <<Const0:i\d+>>   IntConstant 0
## CHECK-DAG:     <<Cond:z\d+>>     NotEqual [<<Const0>>,<<Arg>>]
## CHECK-DAG:                       If [<<Cond>>]

## CHECK-START: int SmaliTests.NotEqualFalseLhs(boolean) instruction_simplifier (after)
## CHECK-DAG:     <<Arg:z\d+>>      ParameterValue
## CHECK-DAG:                       If [<<Arg>>]

.method public static NotEqualFalseLhs(Z)I
  .registers 3

  const v0, 0x0
  const v1, 0x5
  if-ne v0, p0, :return
  const v1, 0x3
  :return
  return v1

.end method

## CHECK-START: int SmaliTests.AddSubConst(int) instruction_simplifier (before)
## CHECK-DAG:     <<ArgValue:i\d+>>  ParameterValue
## CHECK-DAG:     <<Const7:i\d+>>    IntConstant 7
## CHECK-DAG:     <<Const8:i\d+>>    IntConstant 8
## CHECK-DAG:     <<Add:i\d+>>       Add [<<ArgValue>>,<<Const7>>]
## CHECK-DAG:     <<Sub:i\d+>>       Sub [<<Add>>,<<Const8>>]
## CHECK-DAG:                        Return [<<Sub>>]

## CHECK-START: int SmaliTests.AddSubConst(int) instruction_simplifier (after)
## CHECK-DAG:     <<ArgValue:i\d+>>  ParameterValue
## CHECK-DAG:     <<ConstM1:i\d+>>   IntConstant -1
## CHECK-DAG:     <<Add:i\d+>>       Add [<<ArgValue>>,<<ConstM1>>]
## CHECK-DAG:                        Return [<<Add>>]

.method public static AddSubConst(I)I
    .registers 3

    .prologue
    add-int/lit8 v0, p0, 7

    const/16 v1, 8

    sub-int v0, v0, v1

    return v0
.end method

## CHECK-START: int SmaliTests.SubAddConst(int) instruction_simplifier (before)
## CHECK-DAG:     <<ArgValue:i\d+>>  ParameterValue
## CHECK-DAG:     <<Const3:i\d+>>    IntConstant 3
## CHECK-DAG:     <<Const4:i\d+>>    IntConstant 4
## CHECK-DAG:     <<Sub:i\d+>>       Sub [<<ArgValue>>,<<Const3>>]
## CHECK-DAG:     <<Add:i\d+>>       Add [<<Sub>>,<<Const4>>]
## CHECK-DAG:                        Return [<<Add>>]

## CHECK-START: int SmaliTests.SubAddConst(int) instruction_simplifier (after)
## CHECK-DAG:     <<ArgValue:i\d+>>  ParameterValue
## CHECK-DAG:     <<Const1:i\d+>>    IntConstant 1
## CHECK-DAG:     <<Add:i\d+>>       Add [<<ArgValue>>,<<Const1>>]
## CHECK-DAG:                        Return [<<Add>>]

.method public static SubAddConst(I)I
    .registers 2

    .prologue
    const/4 v0, 3

    sub-int v0, p0, v0

    add-int/lit8 v0, v0, 4

    return v0
.end method

## CHECK-START: int SmaliTests.SubSubConst1(int) instruction_simplifier (before)
## CHECK-DAG:     <<ArgValue:i\d+>>  ParameterValue
## CHECK-DAG:     <<Const9:i\d+>>    IntConstant 9
## CHECK-DAG:     <<Const10:i\d+>>   IntConstant 10
## CHECK-DAG:     <<Sub1:i\d+>>      Sub [<<ArgValue>>,<<Const9>>]
## CHECK-DAG:     <<Sub2:i\d+>>      Sub [<<Sub1>>,<<Const10>>]
## CHECK-DAG:                        Return [<<Sub2>>]

## CHECK-START: int SmaliTests.SubSubConst1(int) instruction_simplifier (after)
## CHECK-DAG:     <<ArgValue:i\d+>>  ParameterValue
## CHECK-DAG:     <<ConstM19:i\d+>>  IntConstant -19
## CHECK-DAG:     <<Add:i\d+>>       Add [<<ArgValue>>,<<ConstM19>>]
## CHECK-DAG:                        Return [<<Add>>]

.method public static SubSubConst1(I)I
    .registers 3

    .prologue
    const/16 v1, 9

    sub-int v0, p0, v1

    const/16 v1, 10

    sub-int v0, v0, v1

    return v0
.end method

## CHECK-START: int SmaliTests.SubSubConst2(int) instruction_simplifier (before)
## CHECK-DAG:     <<ArgValue:i\d+>>  ParameterValue
## CHECK-DAG:     <<Const11:i\d+>>   IntConstant 11
## CHECK-DAG:     <<Const12:i\d+>>   IntConstant 12
## CHECK-DAG:     <<Sub1:i\d+>>      Sub [<<Const11>>,<<ArgValue>>]
## CHECK-DAG:     <<Sub2:i\d+>>      Sub [<<Sub1>>,<<Const12>>]
## CHECK-DAG:                        Return [<<Sub2>>]

## CHECK-START: int SmaliTests.SubSubConst2(int) instruction_simplifier (after)
## CHECK-DAG:     <<ArgValue:i\d+>>  ParameterValue
## CHECK-DAG:     <<ConstM1:i\d+>>   IntConstant -1
## CHECK-DAG:     <<Sub:i\d+>>       Sub [<<ConstM1>>,<<ArgValue>>]
## CHECK-DAG:                        Return [<<Sub>>]

.method public static SubSubConst2(I)I
    .registers 3

    .prologue
    rsub-int/lit8 v0, p0, 11

    const/16 v1, 12

    sub-int v0, v0, v1

    return v0
.end method

## CHECK-START: int SmaliTests.SubSubConst3(int) instruction_simplifier (before)
## CHECK-DAG:     <<ArgValue:i\d+>>  ParameterValue
## CHECK-DAG:     <<Const15:i\d+>>   IntConstant 15
## CHECK-DAG:     <<Const16:i\d+>>   IntConstant 16
## CHECK-DAG:     <<Sub1:i\d+>>      Sub [<<ArgValue>>,<<Const16>>]
## CHECK-DAG:     <<Sub2:i\d+>>      Sub [<<Const15>>,<<Sub1>>]
## CHECK-DAG:                        Return [<<Sub2>>]

## CHECK-START: int SmaliTests.SubSubConst3(int) instruction_simplifier (after)
## CHECK-DAG:     <<ArgValue:i\d+>>  ParameterValue
## CHECK-DAG:     <<Const31:i\d+>>   IntConstant 31
## CHECK-DAG:     <<Sub:i\d+>>       Sub [<<Const31>>,<<ArgValue>>]
## CHECK-DAG:                        Return [<<Sub>>]

.method public static SubSubConst3(I)I
    .registers 2

    .prologue
    const/16 v0, 16

    sub-int v0, p0, v0

    rsub-int/lit8 v0, v0, 15

    return v0
.end method

# Test simplification of the `~~var` pattern.
# The transformation tested is implemented in `InstructionSimplifierVisitor::VisitNot`.

## CHECK-START: long SmaliTests.NotNot1(long) instruction_simplifier (before)
## CHECK-DAG:     <<Arg:j\d+>>      ParameterValue
## CHECK-DAG:     <<Not1:j\d+>>     Not [<<Arg>>]
## CHECK-DAG:     <<Not2:j\d+>>     Not [<<Not1>>]
## CHECK-DAG:                       Return [<<Not2>>]

## CHECK-START: long SmaliTests.NotNot1(long) instruction_simplifier (after)
## CHECK-DAG:     <<Arg:j\d+>>      ParameterValue
## CHECK-DAG:                       Return [<<Arg>>]

## CHECK-START: long SmaliTests.NotNot1(long) instruction_simplifier (after)
## CHECK-NOT:                       Not

.method public static NotNot1(J)J
    .registers 4
    .param p0, "arg"    # J

    .prologue
    sget-boolean v0, LMain;->doThrow:Z

    # if (doThrow) throw new Error();
    if-eqz v0, :cond_a
    new-instance v0, Ljava/lang/Error;
    invoke-direct {v0}, Ljava/lang/Error;-><init>()V
    throw v0

  :cond_a
    # return ~~arg
    not-long v0, p0
    not-long v0, v0
    return-wide v0
.end method

## CHECK-START: int SmaliTests.NotNot2(int) instruction_simplifier (before)
## CHECK-DAG:     <<Arg:i\d+>>      ParameterValue
## CHECK-DAG:     <<Not1:i\d+>>     Not [<<Arg>>]
## CHECK-DAG:     <<Not2:i\d+>>     Not [<<Not1>>]
## CHECK-DAG:     <<Add:i\d+>>      Add [<<Not2>>,<<Not1>>]
## CHECK-DAG:                       Return [<<Add>>]

## CHECK-START: int SmaliTests.NotNot2(int) instruction_simplifier (after)
## CHECK-DAG:     <<Arg:i\d+>>      ParameterValue
## CHECK-DAG:     <<Not:i\d+>>      Not [<<Arg>>]
## CHECK-DAG:     <<Add:i\d+>>      Add [<<Arg>>,<<Not>>]
## CHECK-DAG:                       Return [<<Add>>]

## CHECK-START: int SmaliTests.NotNot2(int) instruction_simplifier (after)
## CHECK:                           Not
## CHECK-NOT:                       Not

.method public static NotNot2(I)I
    .registers 3
    .param p0, "arg"    # I

    .prologue
    sget-boolean v1, LMain;->doThrow:Z

    # if (doThrow) throw new Error();
    if-eqz v1, :cond_a
    new-instance v1, Ljava/lang/Error;
    invoke-direct {v1}, Ljava/lang/Error;-><init>()V
    throw v1

  :cond_a
    # temp = ~arg; return temp + ~temp;
    not-int v0, p0
    not-int v1, v0
    add-int/2addr v1, v0

    return v1
.end method

# Test simplification of double Boolean negation. Note that sometimes
# both negations can be removed but we only expect the simplifier to
# remove the second.

## CHECK-START: boolean SmaliTests.NotNotBool(boolean) instruction_simplifier (before)
## CHECK-DAG:     <<Arg:z\d+>>       ParameterValue
## CHECK-DAG:     <<Const1:i\d+>>    IntConstant 1
## CHECK-DAG:     <<Result:z\d+>>    InvokeStaticOrDirect
## CHECK-DAG:     <<NotResult:i\d+>> Xor [<<Result>>,<<Const1>>]
## CHECK-DAG:                        Return [<<NotResult>>]

## CHECK-START: boolean SmaliTests.NotNotBool(boolean) instruction_simplifier (after)
## CHECK-DAG:     <<Arg:z\d+>>       ParameterValue
## CHECK-DAG:     <<Result:z\d+>>    InvokeStaticOrDirect
## CHECK-DAG:     <<NotResult:z\d+>> BooleanNot [<<Result>>]
## CHECK-DAG:                        Return [<<NotResult>>]

## CHECK-START: boolean SmaliTests.NotNotBool(boolean) instruction_simplifier$after_inlining (before)
## CHECK-DAG:     <<Arg:z\d+>>       ParameterValue
## CHECK-DAG:     <<NotArg:z\d+>>    BooleanNot [<<Arg>>]
## CHECK-DAG:     <<NotNotArg:z\d+>> BooleanNot [<<NotArg>>]
## CHECK-DAG:                        Return [<<NotNotArg>>]

## CHECK-START: boolean SmaliTests.NotNotBool(boolean) instruction_simplifier$after_inlining (after)
## CHECK-DAG:     <<Arg:z\d+>>       ParameterValue
## CHECK-DAG:     <<NotArg:z\d+>>    BooleanNot [<<Arg>>]
## CHECK-DAG:                        Return [<<Arg>>]

## CHECK-START: boolean SmaliTests.NotNotBool(boolean) dead_code_elimination$final (after)
## CHECK-DAG:     <<Arg:z\d+>>       ParameterValue
## CHECK-DAG:                        Return [<<Arg>>]

.method public static NegateValue(Z)Z
    .registers 2
    .param p0, "arg"    # Z

    .prologue

    # return !arg
    xor-int/lit8 v0, p0, 0x1
    return v0
.end method


.method public static NotNotBool(Z)Z
    .registers 2
    .param p0, "arg"    # Z

    .prologue
    sget-boolean v0, LMain;->doThrow:Z

    # if (doThrow) throw new Error();
    if-eqz v0, :cond_a
    new-instance v0, Ljava/lang/Error;
    invoke-direct {v0}, Ljava/lang/Error;-><init>()V
    throw v0

  :cond_a
    # return !Negate(arg)
    invoke-static {p0}, LSmaliTests;->NegateValue(Z)Z
    move-result v0
    xor-int/lit8 v0, v0, 0x1

    return v0
.end method


##  CHECK-START: int SmaliTests.$noinline$XorAllOnes(int) instruction_simplifier (before)
##  CHECK-DAG:     <<Arg:i\d+>>      ParameterValue
##  CHECK-DAG:     <<ConstF:i\d+>>   IntConstant -1
##  CHECK-DAG:     <<Xor:i\d+>>      Xor [<<Arg>>,<<ConstF>>]
##  CHECK-DAG:                       Return [<<Xor>>]

##  CHECK-START: int SmaliTests.$noinline$XorAllOnes(int) instruction_simplifier (after)
##  CHECK-DAG:     <<Arg:i\d+>>      ParameterValue
##  CHECK-DAG:     <<Not:i\d+>>      Not [<<Arg>>]
##  CHECK-DAG:                       Return [<<Not>>]

##  CHECK-START: int SmaliTests.$noinline$XorAllOnes(int) instruction_simplifier (after)
##  CHECK-NOT:                       Xor

# Note: D8 already does this optimization.
.method public static $noinline$XorAllOnes(I)I
    .registers 2

    # return arg ^ -1;
    sget-boolean v0, LMain;->doThrow:Z
    if-eqz v0, :cond_a
    new-instance v0, Ljava/lang/Error;
    invoke-direct {v0}, Ljava/lang/Error;-><init>()V
    throw v0

    :cond_a
    xor-int/lit8 v0, p0, -1

    return v0
.end method

#   This is similar to the test-case AddNegs1, but the negations have
#   multiple uses.
#   The transformation tested is implemented in
#   `InstructionSimplifierVisitor::TryMoveNegOnInputsAfterBinop`.
#   The current code won't perform the previous optimization. The
#   transformations do not look at other uses of their inputs. As they don't
#   know what will happen with other uses, they do not take the risk of
#   increasing the register pressure by creating or extending live ranges.

## CHECK-START: int SmaliTests.$noinline$AddNegs2(int, int) instruction_simplifier (before)
## CHECK-DAG:     <<Arg1:i\d+>>     ParameterValue
## CHECK-DAG:     <<Arg2:i\d+>>     ParameterValue
## CHECK-DAG:     <<Neg1:i\d+>>     Neg [<<Arg1>>]
## CHECK-DAG:     <<Neg2:i\d+>>     Neg [<<Arg2>>]
## CHECK-DAG:     <<Add1:i\d+>>     Add [<<Neg1>>,<<Neg2>>]
## CHECK-DAG:     <<Add2:i\d+>>     Add [<<Neg1>>,<<Neg2>>]
## CHECK-DAG:     <<Or:i\d+>>       Or [<<Add1>>,<<Add2>>]
## CHECK-DAG:                       Return [<<Or>>]

## CHECK-START: int SmaliTests.$noinline$AddNegs2(int, int) instruction_simplifier (after)
## CHECK-DAG:     <<Arg1:i\d+>>     ParameterValue
## CHECK-DAG:     <<Arg2:i\d+>>     ParameterValue
## CHECK-DAG:     <<Neg1:i\d+>>     Neg [<<Arg1>>]
## CHECK-DAG:     <<Neg2:i\d+>>     Neg [<<Arg2>>]
## CHECK-DAG:     <<Add1:i\d+>>     Add [<<Neg1>>,<<Neg2>>]
## CHECK-DAG:     <<Add2:i\d+>>     Add [<<Neg1>>,<<Neg2>>]
## CHECK-NOT:                       Neg
## CHECK-DAG:     <<Or:i\d+>>       Or [<<Add1>>,<<Add2>>]
## CHECK-DAG:                       Return [<<Or>>]

## CHECK-START: int SmaliTests.$noinline$AddNegs2(int, int) GVN (after)
## CHECK-DAG:     <<Arg1:i\d+>>     ParameterValue
## CHECK-DAG:     <<Arg2:i\d+>>     ParameterValue
## CHECK-DAG:     <<Neg1:i\d+>>     Neg [<<Arg1>>]
## CHECK-DAG:     <<Neg2:i\d+>>     Neg [<<Arg2>>]
## CHECK-DAG:     <<Add:i\d+>>      Add [<<Neg1>>,<<Neg2>>]
## CHECK-DAG:     <<Or:i\d+>>       Or [<<Add>>,<<Add>>]
## CHECK-DAG:                       Return [<<Or>>]

# Note: D8 already does GVN so this has to be in Smali.
.method public static $noinline$AddNegs2(II)I
    # int temp1 = -arg1;
    # int temp2 = -arg2;
    # return (temp1 + temp2) | (temp1 + temp2);
    .registers 6

    neg-int v0, p0
    neg-int v1, p1

    add-int v2, v0, v1
    add-int v3, v0, v1

    or-int/2addr v2, v3

    return v2
.end method

#   This is similar to the test-case AddNeg1, but the negation has two uses.
#   The transformation tested is implemented in `InstructionSimplifierVisitor::VisitAdd`.
#   The current code won't perform the previous optimization. The
#   transformations do not look at other uses of their inputs. As they don't
#   know what will happen with other uses, they do not take the risk of
#   increasing the register pressure by creating or extending live ranges.

##  CHECK-START: long SmaliTests.$noinline$AddNeg2(long, long) instruction_simplifier (before)
##  CHECK-DAG:     <<Arg1:j\d+>>     ParameterValue
##  CHECK-DAG:     <<Arg2:j\d+>>     ParameterValue
##  CHECK-DAG:     <<Neg:j\d+>>      Neg [<<Arg2>>]
##  CHECK-DAG:     <<Add1:j\d+>>     Add [<<Arg1>>,<<Neg>>]
##  CHECK-DAG:     <<Add2:j\d+>>     Add [<<Arg1>>,<<Neg>>]
##  CHECK-DAG:     <<Res:j\d+>>      Or [<<Add1>>,<<Add2>>]
##  CHECK-DAG:                       Return [<<Res>>]

##  CHECK-START: long SmaliTests.$noinline$AddNeg2(long, long) instruction_simplifier (after)
##  CHECK-DAG:     <<Arg1:j\d+>>     ParameterValue
##  CHECK-DAG:     <<Arg2:j\d+>>     ParameterValue
##  CHECK-DAG:     <<Neg:j\d+>>      Neg [<<Arg2>>]
##  CHECK-DAG:     <<Add1:j\d+>>     Add [<<Arg1>>,<<Neg>>]
##  CHECK-DAG:     <<Add2:j\d+>>     Add [<<Arg1>>,<<Neg>>]
##  CHECK-DAG:     <<Res:j\d+>>      Or [<<Add1>>,<<Add2>>]
##  CHECK-DAG:                       Return [<<Res>>]

##  CHECK-START: long SmaliTests.$noinline$AddNeg2(long, long) instruction_simplifier (after)
##  CHECK-NOT:                       Sub

# Note: D8 already does GVN so this has to be in Smali.
.method public static $noinline$AddNeg2(JJ)J
    # long temp = -arg2;
    # return (arg1 + temp) | (arg1 + temp);
    .registers 10
    neg-long v0, p2

    add-long v2, p0, v0
    add-long v4, p0, v0

    or-long/2addr v2, v4
    return-wide v2
.end method

#   This is similar to the test-case NegSub1, but the subtraction has
#   multiple uses.
#   The transformation tested is implemented in `InstructionSimplifierVisitor::VisitNeg`.
#   The current code won't perform the previous optimization. The
#   transformations do not look at other uses of their inputs. As they don't
#   know what will happen with other uses, they do not take the risk of
#   increasing the register pressure by creating or extending live ranges.

##  CHECK-START: int SmaliTests.$noinline$NegSub2(int, int) instruction_simplifier (before)
##  CHECK-DAG:     <<Arg1:i\d+>>     ParameterValue
##  CHECK-DAG:     <<Arg2:i\d+>>     ParameterValue
##  CHECK-DAG:     <<Sub:i\d+>>      Sub [<<Arg1>>,<<Arg2>>]
##  CHECK-DAG:     <<Neg1:i\d+>>     Neg [<<Sub>>]
##  CHECK-DAG:     <<Neg2:i\d+>>     Neg [<<Sub>>]
##  CHECK-DAG:     <<Or:i\d+>>       Or [<<Neg1>>,<<Neg2>>]
##  CHECK-DAG:                       Return [<<Or>>]

##  CHECK-START: int SmaliTests.$noinline$NegSub2(int, int) instruction_simplifier (after)
##  CHECK-DAG:     <<Arg1:i\d+>>     ParameterValue
##  CHECK-DAG:     <<Arg2:i\d+>>     ParameterValue
##  CHECK-DAG:     <<Sub:i\d+>>      Sub [<<Arg1>>,<<Arg2>>]
##  CHECK-DAG:     <<Neg1:i\d+>>     Neg [<<Sub>>]
##  CHECK-DAG:     <<Neg2:i\d+>>     Neg [<<Sub>>]
##  CHECK-DAG:     <<Or:i\d+>>       Or [<<Neg1>>,<<Neg2>>]
##  CHECK-DAG:                       Return [<<Or>>]

# Note: D8 already does GVN so this has to be in Smali.
.method public static $noinline$NegSub2(II)I
    # int temp = arg1 - arg2;
    # return -temp | -temp;
    .registers 5
    sub-int v0, p0, p1

    neg-int v1, v0
    neg-int v2, v0

    or-int/2addr v1, v2
    return v1
.end method

#   Test simplification of the `~~var` pattern.
#   The transformation tested is implemented in `InstructionSimplifierVisitor::VisitNot`.

##  CHECK-START: long SmaliTests.$noinline$NotNot1(long) instruction_simplifier (before)
##  CHECK-DAG:     <<Arg:j\d+>>       ParameterValue
##  CHECK-DAG:     <<ConstNeg1:j\d+>> LongConstant -1
##  CHECK-DAG:     <<Not1:j\d+>>      Xor [<<Arg>>,<<ConstNeg1>>]
##  CHECK-DAG:     <<Not2:j\d+>>      Xor [<<Not1>>,<<ConstNeg1>>]
##  CHECK-DAG:                        Return [<<Not2>>]

##  CHECK-START: long SmaliTests.$noinline$NotNot1(long) instruction_simplifier (after)
##  CHECK-DAG:     <<Arg:j\d+>>      ParameterValue
##  CHECK-DAG:                       Return [<<Arg>>]

##  CHECK-START: long SmaliTests.$noinline$NotNot1(long) instruction_simplifier (after)
##  CHECK-NOT:                       Xor

# Note: D8 just use Not-Long so we need to write this in Smali.
.method public static $noinline$NotNot1(J)J
    # return ~~arg;
    .registers 6
    const-wide/16 v2, -0x1

    xor-long v0, p0, v2
    xor-long/2addr v0, v2

    return-wide v0
.end method

##  CHECK-START: int SmaliTests.$noinline$NotNot2(int) instruction_simplifier (before)
##  CHECK-DAG:     <<Arg:i\d+>>       ParameterValue
##  CHECK-DAG:     <<ConstNeg1:i\d+>> IntConstant -1
##  CHECK-DAG:     <<Not1:i\d+>>      Xor [<<Arg>>,<<ConstNeg1>>]
##  CHECK-DAG:     <<Not2:i\d+>>      Xor [<<Not1>>,<<ConstNeg1>>]
##  CHECK-DAG:     <<Add:i\d+>>       Add [<<Not2>>,<<Not1>>]
##  CHECK-DAG:                        Return [<<Add>>]

##  CHECK-START: int SmaliTests.$noinline$NotNot2(int) instruction_simplifier (after)
##  CHECK-DAG:     <<Arg:i\d+>>      ParameterValue
##  CHECK-DAG:     <<Not:i\d+>>      Not [<<Arg>>]
##  CHECK-DAG:     <<Add:i\d+>>      Add [<<Arg>>,<<Not>>]
##  CHECK-DAG:                       Return [<<Add>>]

##  CHECK-START: int SmaliTests.$noinline$NotNot2(int) instruction_simplifier (after)
##  CHECK:                           Not
##  CHECK-NOT:                       Not

##  CHECK-START: int SmaliTests.$noinline$NotNot2(int) instruction_simplifier (after)
##  CHECK-NOT:                       Xor

# Note: D8 just use Not-Int so we need to write this in Smali.
.method public static $noinline$NotNot2(I)I
    # int temp = ~arg;
    # return temp + ~temp;
    .registers 3

    xor-int/lit8 v0, p0, -0x1
    xor-int/lit8 v1, v0, -0x1
    add-int/2addr v1, v0

    return v1
.end method

#  This is similar to the test-case SubNeg1, but the negation has
#  multiple uses.
#  The transformation tested is implemented in `InstructionSimplifierVisitor::VisitSub`.
#  The current code won't perform the previous optimization. The
#  transformations do not look at other uses of their inputs. As they don't
#  know what will happen with other uses, they do not take the risk of
#  increasing the register pressure by creating or extending live ranges.

## CHECK-START: int SmaliTests.$noinline$SubNeg2(int, int) instruction_simplifier (before)
## CHECK-DAG:     <<Arg1:i\d+>>     ParameterValue
## CHECK-DAG:     <<Arg2:i\d+>>     ParameterValue
## CHECK-DAG:     <<Neg:i\d+>>      Neg [<<Arg1>>]
## CHECK-DAG:     <<Sub1:i\d+>>     Sub [<<Neg>>,<<Arg2>>]
## CHECK-DAG:     <<Sub2:i\d+>>     Sub [<<Neg>>,<<Arg2>>]
## CHECK-DAG:     <<Or:i\d+>>       Or [<<Sub1>>,<<Sub2>>]
## CHECK-DAG:                       Return [<<Or>>]

## CHECK-START: int SmaliTests.$noinline$SubNeg2(int, int) instruction_simplifier (after)
## CHECK-DAG:     <<Arg1:i\d+>>     ParameterValue
## CHECK-DAG:     <<Arg2:i\d+>>     ParameterValue
## CHECK-DAG:     <<Neg:i\d+>>      Neg [<<Arg1>>]
## CHECK-DAG:     <<Sub1:i\d+>>     Sub [<<Neg>>,<<Arg2>>]
## CHECK-DAG:     <<Sub2:i\d+>>     Sub [<<Neg>>,<<Arg2>>]
## CHECK-DAG:     <<Or:i\d+>>       Or [<<Sub1>>,<<Sub2>>]
## CHECK-DAG:                       Return [<<Or>>]

## CHECK-START: int SmaliTests.$noinline$SubNeg2(int, int) instruction_simplifier (after)
## CHECK-NOT:                       Add

# Note: D8 already does GVN so this has to be in Smali.
.method public static $noinline$SubNeg2(II)I
    # int temp = -arg1;
    # return (temp - arg2) | (temp - arg2);
    .registers 5
    sget-boolean v1, LMain;->doThrow:Z

    neg-int v0, p0

    sub-int v1, v0, p1
    sub-int v2, v0, p1

    or-int/2addr v1, v2

    return v1
.end method

#  
#  Test simplification of double Boolean negation. Note that sometimes
#  both negations can be removed but we only expect the simplifier to
#  remove the second.
#  

##  CHECK-START: boolean SmaliTests.$noinline$NotNotBool(boolean) instruction_simplifier (before)
##  CHECK-DAG:     <<Arg:z\d+>>       ParameterValue
##  CHECK-DAG:     <<Const1:i\d+>>    IntConstant 0
##  CHECK-DAG:     <<Result:z\d+>>    InvokeStaticOrDirect method_name:SmaliTests.NegateValueBranch
##  CHECK-DAG:     <<NotResult:z\d+>> NotEqual [<<Result>>,<<Const1>>]
##  CHECK-DAG:                        If [<<NotResult>>]

##  CHECK-START: boolean SmaliTests.$noinline$NotNotBool(boolean) instruction_simplifier (after)
##  CHECK-NOT:                        NotEqual

##  CHECK-START: boolean SmaliTests.$noinline$NotNotBool(boolean) instruction_simplifier (after)
##  CHECK-DAG:     <<Arg:z\d+>>       ParameterValue
##  CHECK-DAG:     <<Result:z\d+>>    InvokeStaticOrDirect method_name:SmaliTests.NegateValueBranch
##  CHECK-DAG:     <<Const0:i\d+>>    IntConstant 0
##  CHECK-DAG:     <<Const1:i\d+>>    IntConstant 1
##  CHECK-DAG:     <<Phi:i\d+>>       Phi [<<Const1>>,<<Const0>>]
##  CHECK-DAG:                        Return [<<Phi>>]

##  CHECK-START: boolean SmaliTests.$noinline$NotNotBool(boolean) instruction_simplifier$after_inlining (before)
##  CHECK-DAG:     <<Arg:z\d+>>       ParameterValue
##  CHECK-NOT:                        BooleanNot [<<Arg>>]
##  CHECK-NOT:                        Phi

##  CHECK-START: boolean SmaliTests.$noinline$NotNotBool(boolean) instruction_simplifier$after_inlining (before)
##  CHECK-DAG:     <<Arg:z\d+>>       ParameterValue
##  CHECK-DAG:     <<Const0:i\d+>>    IntConstant 0
##  CHECK-DAG:     <<Const1:i\d+>>    IntConstant 1
##  CHECK-DAG:     <<Sel:i\d+>>       Select [<<Const1>>,<<Const0>>,<<Arg>>]
##  CHECK-DAG:     <<Sel2:i\d+>>      Select [<<Const1>>,<<Const0>>,<<Sel>>]
##  CHECK-DAG:                        Return [<<Sel2>>]

##  CHECK-START: boolean SmaliTests.$noinline$NotNotBool(boolean) instruction_simplifier$after_inlining (after)
##  CHECK-DAG:     <<Arg:z\d+>>       ParameterValue
##  CHECK:                            BooleanNot [<<Arg>>]
##  CHECK-NEXT:                       Goto

##  CHECK-START: boolean SmaliTests.$noinline$NotNotBool(boolean) instruction_simplifier$after_inlining (after)
##  CHECK-NOT:                        Select

##  CHECK-START: boolean SmaliTests.$noinline$NotNotBool(boolean) dead_code_elimination$final (after)
##  CHECK-DAG:     <<Arg:z\d+>>       ParameterValue
##  CHECK-NOT:                        BooleanNot [<<Arg>>]
##  CHECK-DAG:                        Return [<<Arg>>]

.method public static $noinline$NotNotBool(Z)Z
    # if (doThrow) { throw new Error(); }
    # return !(NegateValue(arg));
    .registers 2
    sget-boolean v0, LMain;->doThrow:Z

    if-eqz v0, :cond_a
    new-instance v0, Ljava/lang/Error;
    invoke-direct {v0}, Ljava/lang/Error;-><init>()V
    throw v0

    :cond_a
    invoke-static {p0}, LSmaliTests;->NegateValueBranch(Z)Z
    move-result v0
    if-nez v0, :cond_12

    const/4 v0, 0x1
    :goto_11
    return v0

    :cond_12
    const/4 v0, 0x0

    goto :goto_11
.end method

.method public static NegateValueBranch(Z)Z
    # if (doThrow) { throw new Error(); }
    # return !(NegateValue(arg));
    .registers 2
    if-nez p0, :cond_4

    const/4 v0, 0x1

    :goto_3
    return v0

    :cond_4
    const/4 v0, 0x0

    goto :goto_3
.end method
