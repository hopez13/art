/*
 * Copyright (C) 2022 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

public class Main {
    public static void main(String[] args) throws Exception {
        assertEquals(40, $noinline$testEliminateBranch(20, 40));
        assertEquals(30, $noinline$testEliminateBranch(20, 10));
        assertEquals(40, $noinline$testEliminateBranchTwiceInARow(20, 40));
        assertEquals(30, $noinline$testEliminateBranchTwiceInARow(20, 10));
        assertEquals(40, $noinline$testEliminateBranchThreePredecessors(20, 40));
        assertEquals(30, $noinline$testEliminateBranchThreePredecessors(20, 10));
        assertEquals(40, $noinline$testEliminateBranchOppositeCondition(20, 40));
        assertEquals(30, $noinline$testEliminateBranchOppositeCondition(20, 10));
        assertEquals(40, $noinline$testEliminateBranchParameter(20, 40, 20 < 40));
        assertEquals(30, $noinline$testEliminateBranchParameter(20, 10, 20 < 10));
        assertEquals(40, $noinline$testEliminateBranchParameterReverseCondition(20, 40, 20 < 40));
        assertEquals(30, $noinline$testEliminateBranchParameterReverseCondition(20, 10, 20 < 10));
    }

    private static int $noinline$emptyMethod(int a) {
        return a;
    }

    /// CHECK-START: int Main.$noinline$testEliminateBranch(int, int) dead_code_elimination$after_gvn (before)
    /// CHECK:     If
    /// CHECK:     If

    /// CHECK-START: int Main.$noinline$testEliminateBranch(int, int) dead_code_elimination$after_gvn (after)
    /// CHECK:     If
    /// CHECK-NOT: If
    private static int $noinline$testEliminateBranch(int a, int b) {
        int result = 0;
        if (a < b) {
            $noinline$emptyMethod(a + b);
        } else {
            $noinline$emptyMethod(a - b);
        }
        if (a < b) {
            result += $noinline$emptyMethod(a * 2);
        } else {
            result += $noinline$emptyMethod(b * 3);
        }
        return result;
    }

    /// CHECK-START: int Main.$noinline$testEliminateBranchTwiceInARow(int, int) dead_code_elimination$after_gvn (before)
    /// CHECK:     If
    /// CHECK:     If
    /// CHECK:     If

    /// CHECK-START: int Main.$noinline$testEliminateBranchTwiceInARow(int, int) dead_code_elimination$after_gvn (after)
    /// CHECK:     If
    /// CHECK-NOT: If
    private static int $noinline$testEliminateBranchTwiceInARow(int a, int b) {
        int result = 0;
        if (a < b) {
            $noinline$emptyMethod(a + b);
        } else {
            $noinline$emptyMethod(a - b);
        }
        if (a < b) {
            $noinline$emptyMethod(a * 2);
        } else {
            $noinline$emptyMethod(b * 3);
        }
        if (a < b) {
            result += $noinline$emptyMethod(40);
        } else {
            result += $noinline$emptyMethod(30);
        }
        return result;
    }

    /// CHECK-START: int Main.$noinline$testEliminateBranchThreePredecessors(int, int) dead_code_elimination$after_gvn (before)
    /// CHECK:     If
    /// CHECK:     If
    /// CHECK:     If

    /// CHECK-START: int Main.$noinline$testEliminateBranchThreePredecessors(int, int) dead_code_elimination$after_gvn (after)
    /// CHECK:     If
    /// CHECK:     If
    /// CHECK-NOT: If
    private static int $noinline$testEliminateBranchThreePredecessors(int a, int b) {
        int result = 0;
        if (a < b) {
            $noinline$emptyMethod(a + b);
        } else {
            if (b < 5) {
                $noinline$emptyMethod(a - b);
            } else {
                $noinline$emptyMethod(a * b);
            }
        }
        if (a < b) {
            result += $noinline$emptyMethod(a * 2);
        } else {
            result += $noinline$emptyMethod(b * 3);
        }
        return result;
    }

    // Note that we can perform this optimization in dead_code_elimination$initial since we don't
    // rely on gvn to de-duplicate the values.

    /// CHECK-START: int Main.$noinline$testEliminateBranchOppositeCondition(int, int) dead_code_elimination$initial (before)
    /// CHECK:     If
    /// CHECK:     If

    /// CHECK-START: int Main.$noinline$testEliminateBranchOppositeCondition(int, int) dead_code_elimination$initial (after)
    /// CHECK:     If
    /// CHECK-NOT: If
    private static int $noinline$testEliminateBranchOppositeCondition(int a, int b) {
        int result = 0;
        if (a < b) {
            $noinline$emptyMethod(a + b);
        } else {
            $noinline$emptyMethod(a - b);
        }
        if (a >= b) {
            result += $noinline$emptyMethod(b * 3);
        } else {
            result += $noinline$emptyMethod(a * 2);
        }
        return result;
    }

    // In this scenario, we have a BooleanNot before the If instructions so we have to wait until
    // the following pass to perform the optimization. The BooleanNot is dead at this time (even
    // when starting DCE), but RemoveDeadInstructions runs after SimplifyIfs so the optimization
    // doesn't trigger.

    /// CHECK-START: int Main.$noinline$testEliminateBranchParameter(int, int, boolean) dead_code_elimination$initial (before)
    /// CHECK:     BooleanNot
    /// CHECK:     If
    /// CHECK:     BooleanNot
    /// CHECK:     If

    /// CHECK-START: int Main.$noinline$testEliminateBranchParameter(int, int, boolean) dead_code_elimination$initial (after)
    /// CHECK:     If
    /// CHECK:     Phi
    /// CHECK:     If

    /// CHECK-START: int Main.$noinline$testEliminateBranchParameter(int, int, boolean) dead_code_elimination$initial (after)
    /// CHECK-NOT: BooleanNot

    /// CHECK-START: int Main.$noinline$testEliminateBranchParameter(int, int, boolean) dead_code_elimination$after_gvn (before)
    /// CHECK:     If
    /// CHECK:     Phi
    /// CHECK:     If

    /// CHECK-START: int Main.$noinline$testEliminateBranchParameter(int, int, boolean) dead_code_elimination$after_gvn (after)
    /// CHECK:     If
    /// CHECK-NOT: If
    private static int $noinline$testEliminateBranchParameter(int a, int b, boolean condition) {
        int result = 0;
        if (condition) {
            $noinline$emptyMethod(a + b);
        } else {
            $noinline$emptyMethod(a - b);
        }
        if (condition) {
            result += $noinline$emptyMethod(a * 2);
        } else {
            result += $noinline$emptyMethod(b * 3);
        }
        return result;
    }

    // Same here, we do it in dead_code_elimination$initial since GVN is not needed.

    /// CHECK-START: int Main.$noinline$testEliminateBranchParameterReverseCondition(int, int, boolean) dead_code_elimination$initial (before)
    /// CHECK:     If
    /// CHECK:     If

    /// CHECK-START: int Main.$noinline$testEliminateBranchParameterReverseCondition(int, int, boolean) dead_code_elimination$initial (after)
    /// CHECK:     If
    /// CHECK-NOT: If
    private static int $noinline$testEliminateBranchParameterReverseCondition(int a, int b, boolean condition) {
        int result = 0;
        if (!condition) {
            $noinline$emptyMethod(a + b);
        } else {
            $noinline$emptyMethod(a - b);
        }
        if (!condition) {
            result += $noinline$emptyMethod(b * 3);
        } else {
            result += $noinline$emptyMethod(a * 2);
        }
        return result;
    }

    public static void assertEquals(int expected, int result) {
        if (expected != result) {
            throw new Error("Expected: " + expected + ", found: " + result);
        }
    }
}
