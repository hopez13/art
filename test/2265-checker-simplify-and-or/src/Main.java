/*
 * Copyright (C) 2023 The Android Open Source Project
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
    public static void main(String[] args) {
        $noinline$testTwoBooleansIntoAnd(false, false);
        $noinline$testThreeBooleansIntoAnd(false, false, false);

        $noinline$testTwoBooleansIntoOr(false, false);
        $noinline$testThreeBooleansIntoOr(false, false, false);
    }
    
    /// CHECK-START: void Main.$noinline$testTwoBooleansIntoAnd(boolean, boolean) dead_code_elimination$initial (before)
    /// CHECK-NOT: And

    /// CHECK-START: void Main.$noinline$testTwoBooleansIntoAnd(boolean, boolean) dead_code_elimination$initial (before)
    /// CHECK:     If
    /// CHECK:     If
    /// CHECK-NOT: If

    /// CHECK-START: void Main.$noinline$testTwoBooleansIntoAnd(boolean, boolean) dead_code_elimination$initial (after)
    /// CHECK:     And
    /// CHECK-NOT: And

    /// CHECK-START: void Main.$noinline$testTwoBooleansIntoAnd(boolean, boolean) dead_code_elimination$initial (after)
    /// CHECK:     If
    /// CHECK-NOT: If
    private static void $noinline$testTwoBooleansIntoAnd(boolean b1, boolean b2) {
        if (b1 && b2) {
            $noinline$emptyMethod();
        } else {
            $noinline$emptyMethod2();
        }
    }

    /// CHECK-START: void Main.$noinline$testThreeBooleansIntoAnd(boolean, boolean, boolean) dead_code_elimination$initial (before)
    /// CHECK-NOT: And

    /// CHECK-START: void Main.$noinline$testThreeBooleansIntoAnd(boolean, boolean, boolean) dead_code_elimination$initial (before)
    /// CHECK:     If
    /// CHECK:     If
    /// CHECK:     If
    /// CHECK-NOT: If

    /// CHECK-START: void Main.$noinline$testThreeBooleansIntoAnd(boolean, boolean, boolean) dead_code_elimination$initial (after)
    /// CHECK:     And
    /// CHECK:     And
    /// CHECK-NOT: And

    /// CHECK-START: void Main.$noinline$testThreeBooleansIntoAnd(boolean, boolean, boolean) dead_code_elimination$initial (after)
    /// CHECK:     If
    /// CHECK-NOT: If
    private static void $noinline$testThreeBooleansIntoAnd(boolean b1, boolean b2, boolean b3) {
        if (b1 && b2 && b3) {
            $noinline$emptyMethod();
        } else {
            $noinline$emptyMethod2();
        }
    }

    /// CHECK-START: void Main.$noinline$testTwoBooleansIntoOr(boolean, boolean) dead_code_elimination$initial (before)
    /// CHECK-NOT: Or

    /// CHECK-START: void Main.$noinline$testTwoBooleansIntoOr(boolean, boolean) dead_code_elimination$initial (before)
    /// CHECK:     If
    /// CHECK:     If
    /// CHECK-NOT: If

    /// CHECK-START: void Main.$noinline$testTwoBooleansIntoOr(boolean, boolean) dead_code_elimination$initial (after)
    /// CHECK:     Or
    /// CHECK-NOT: Or

    /// CHECK-START: void Main.$noinline$testTwoBooleansIntoOr(boolean, boolean) dead_code_elimination$initial (after)
    /// CHECK:     If
    /// CHECK-NOT: If
    private static void $noinline$testTwoBooleansIntoOr(boolean b1, boolean b2) {
        if (b1 || b2) {
            $noinline$emptyMethod();
        } else {
            $noinline$emptyMethod2();
        }
    }

    /// CHECK-START: void Main.$noinline$testThreeBooleansIntoOr(boolean, boolean, boolean) dead_code_elimination$initial (before)
    /// CHECK-NOT: Or

    /// CHECK-START: void Main.$noinline$testThreeBooleansIntoOr(boolean, boolean, boolean) dead_code_elimination$initial (before)
    /// CHECK:     If
    /// CHECK:     If
    /// CHECK:     If
    /// CHECK-NOT: If

    /// CHECK-START: void Main.$noinline$testThreeBooleansIntoOr(boolean, boolean, boolean) dead_code_elimination$initial (after)
    /// CHECK:     Or
    /// CHECK:     Or
    /// CHECK-NOT: Or

    /// CHECK-START: void Main.$noinline$testThreeBooleansIntoOr(boolean, boolean, boolean) dead_code_elimination$initial (after)
    /// CHECK:     If
    /// CHECK-NOT: If
    private static void $noinline$testThreeBooleansIntoOr(boolean b1, boolean b2, boolean b3) {
        if (b1 || b2 || b3) {
            $noinline$emptyMethod();
        } else {
            $noinline$emptyMethod2();
        }
    }

    private static void $noinline$emptyMethod() {}
    private static void $noinline$emptyMethod2() {}
}
