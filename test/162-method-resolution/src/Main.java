/*
 * Copyright (C) 2017 The Android Open Source Project
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

import java.lang.reflect.Method;

public class Main {
    public static void main(String[] args) {
        try {
            /*
             * Test1
             * -----
             * Tested functions:
             *     class Test1Base                      {
             *         public void foo() { ... }
             *     }
             *     class Test1Derived extends Test1Base {
             *         private void foo() { ... }
             *         ...
             *     }
             * Tested invokes:
             *     invoke-direct  Test1Derived.foo()V   from Test1Derived in first dex file
             *     invoke-virtual Test1Derived.foo()V   from Test1User    in second dex file
             *     invoke-virtual Test1Derived.foo()V   from Test1User2   in first dex file
             *
             * Previously, the behavior was very odd because the lookups for direct and
             * virtual methods were independent but results were stored in a single slot
             * in the DexCache method array and then retrieved from there without checking
             * the resolution kind. Thus, the first invoke-direct stored the private
             * Test1Derived.foo() in the DexCache and the attempt to use invoke-virtual
             * from the same dex file (by Test1User2) would throw ICCE. However, the same
             * invoke-virtual from a different dex file (by Test1User) would ignore the
             * direct method Test1Derived.foo() and find the Test1Base.foo() and call it.
             *
             * The method lookup has been changed and we now consistently find the private
             * Derived.foo() and throw ICCE for both invoke-virtual calls.
             *
             * Note that for the Java files to compile the test case, we need Test1User
             * and Test1User2 to see the class Test1Derived defining a public foo()V but
             * we need a different Test1Derived with a private foo()V for the final test,
             * so we define Test1Derived in src/ and src2/. The Test1Derived in src2/ with
             * private foo()V () needs to derive from a Test1Base class that defines a
             * public foo()V in the final test but it cannot reduce its visibility and
             * therefore needs to be compiled with a Test1Base that does not define it,
             * so we define Test1Base in src2/ and src3/. Files:
             *   src/Test1User2.java         - calls invoke-virtual Test1Derived.foo().
             *   src-multidex/Test1User.java - calls invoke-virtual Test1Derived.foo().
             *   src/Test1Derived.java       - defines public foo()V, not used in the test.
             *   src2/Test1Derived.java      - defines private foo()V, calls it with invoke-direct.
             *   src2/Test1Base.java         - does not define foo()V, not used in the test.
             *   src3/Test1Base.java         - defines public foo()V.
             */
            invokeUserTest("Test1Derived");
            invokeUserTest("Test1User");
            invokeUserTest("Test1User2");

            /*
             * Test2
             * -----
             * Tested functions:
             *     class Test2Base {
             *         public static void foo() { ... }
             *     }
             *     interface Test2Interface {
             *         default void foo() { ... }  // default: avoid subclassing Test2Derived.
             *     }
             *     class Test2Derived extends Test2Base implements Test2Interface { }
             * Tested invokes:
             *     invoke-virtual Test2Derived.foo()V   from Test2User  in first dex file
             *     invoke-static  Test2Derived.foo()V   from Test2User2 in first dex file
             *
             * Previously, due to different lookup types and multi-threaded verification,
             * it was undeterministic which method ended up in the DexCache, so this test
             * was flaky, sometimes erroneously executing the Test2Base.foo().
             *
             * The method lookup has been changed and we now consistently find the
             * Test2Base.foo()V over the method from the interface, in line with the RI.
             *
             * Similarly to Test1, we need to use src/, src2/ and src3/. Files:
             *   src/Test2User2.java         - calls invoke-static Test2Derived.foo()
             *   src/Test2Derived.java       - defines public static foo()V, not used in the test.
             *   src2/Test2User.java         - calls invoke-virtual Test2Derived.foo()
             *   src2/Test2Base.java         - does not define foo()V, not used in the test.
             *   src2/Test2Interface.java    - defines default foo()V.
             *   src2/Test2Derived.java      - extends Test2Derived, implements Test2Interface.
             *   src3/Test2Base.java         - defines public static foo()V.
             *
             * TODO: Add individual deterministic tests?
             */
            invokeUserTest("Test2User");
            invokeUserTest("Test2User2");

            /*
             * Test3
             * -----
             * Tested functions:
             *     class Test3Base {
             *         public static void foo() { ... }
             *     }
             *     interface Test3Interface {
             *         default void foo() { ... }  // default: avoid subclassing Test2Derived.
             *     }
             *     class Test3Derived extends Test3Base implements Test3Interface { }
             * Tested invokes:
             *     invoke-virtual Test3Derived.foo()V   from Test2User  in second dex file
             *
             * This is a variation of Test2 where we do not test invoke-static and test the
             * invoke-interface from the second dex file to avoid the effects of the DexCache.
             *
             * Previously the invoke-virtual would resolve to the Test3Interface.foo()V but
             * it now resolves to Test3Base.foo()V and throws ICCE in line with the RI.
             *
             * Files:
             *   src-multidex/Test3User.java - calls invoke-virtual Test3Derived.foo()
             *   src/Test3Base.java          - does not define foo()V, not used in the test.
             *   src/Test3Interface.java     - defines default foo()V.
             *   src/Test3Derived.java       - extends Test2Derived, implements Test2Interface.
             *   src2/Test3Base.java         - defines public static foo()V.
             */
            invokeUserTest("Test3User");

            /*
             * Test4
             * -----
             * Tested functions:
             *     interface Test4Interface {
             *       // Not declaring toString().
             *     }
             * Tested invokes:
             *     invoke-interface Test4Interface.toString()Ljava/lang/String; in first dex file
             *
             * The RI resolves the call to java.lang.Object.toString() and executes it.
             * ART used to resolve it in a secondary resolution attempt only to distinguish
             * between ICCE and NSME and then throw ICCE. We now allow the call to proceed.
             *
             * We need to compile Test4User with Test4Interface declaring toString()
             * but execute the test with Test4Interface that does not declare it. Files:
             *   src/Test4User.java          - calls invoke-interface SomeInterface.toString().
             *   src/Test4Interface.java     - declares toString(), not used in the test.
             *   src/Test4Derived.java       - extends Test4Interface.
             *   src2/Test4Interface.java    - does not declare toString().
             */
            invokeUserTest("Test4User");

            // TODO: Test invoke-interface on a non-interface class after the MethodId
            // has been resolved with invoke-virtual.

            // TODO: Test invoke-virtual on an interface class after the MethodId
            // has been resolved with invoke-interface to a public non-static method in Object.

            // TODO: How to test that interface method resolution returns the unique
            // maximally-specific non-abstract superinterface method if there is one?
            // Maybe reflection? (This is not even implemented yet!)
        } catch (Throwable t) {
            t.printStackTrace(System.out);
        }
    }

    private static void invokeUserTest(String userName) throws Exception {
        System.out.println("Calling " + userName + ".test():");
        try {
            Class<?> user = Class.forName(userName);
            Method utest = user.getDeclaredMethod("test");
            utest.invoke(null);
        } catch (Throwable t) {
            System.out.println("Caught " + t.getClass().getName());
            for (Throwable c = t.getCause(); c != null; c = c.getCause()) {
                System.out.println("  caused by " + c.getClass().getName());
            }
        }
    }
}
