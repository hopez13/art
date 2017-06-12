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
             * Test invokes referencing private Derived.foo() where Derived extends Base with
             * public Base.foo(). First, Derived correctly uses invoke-direct Derived.foo()
             * and then User and User2 try to use invoke-virtual Derived.foo(). This test
             * has a multi-dex setup where User2 and Derived are in the same dex file and
             * User is in a different one.
             *
             * Previously, the behavior was very odd because the lookup for direct and virtual
             * methods was independent but results were stored in a single slot in the DexCache
             * method array and then retrieved from there without checking the resolution kind.
             * Thus, the first invoke-direct stored the private Derived.foo() in the dex cache
             * and the attempt to use invoke-virtual from the same dex file (by User2) would
             * throw ICCE. However, the same invoke-virtual from a different dex file (by User)
             * would ignore the direct method Derived.foo() and find the Base.foo() and call it.
             *
             * The method lookup has been changed and we now consistently find the private
             * Derived.foo() and throw ICCE for both invoke-virtual calls.
             *
             * Note that for the Java files to compile the test case, we need User and User2 to
             * see the class Derived defining a public foo()V but we need a different Derived
             * with a private foo()V for the final test, so we define Derived in src/ and src2/.
             * The Derived with private foo()V (in src2/) needs to derive from a Base class that
             * defines a public foo()V in the final test but it cannot reduce its visibility and
             * therefore needs to be compiled with a Base that does not define it, so we define
             * Base in src/ and src2/. Files:
             *   src/User2.java         - calls invoke-virtual Derived.foo().
             *   src-multidex/User.java - calls invoke-virtual Derived.foo().
             *   src/Derived.java       - defines public foo()V, not used in the test.
             *   src2/Derived.java      - defines private foo()V and calls it with invoke-direct.
             *   src2/Base.java         - does not define foo()V, not used in the test.
             *   src3/Base.java         - defines public foo()V.
             */
            invokeUserTest("Derived");
            invokeUserTest("User");
            invokeUserTest("User2");

            /*
             * Test invoke-virtual and invoke-static method resolution with the same name and
             * signature on a class that does not declare such method but extends a superclass
             * with a public static method with that name and signature, and implements an
             * interface that has a default method with that name and signature.
             *
             * TODO: Investigate why the behavior was previously flaky!
             *       Maybe change the test to expose the failure deterministically.
             *       (Then describe the test better.)
             */
            invokeUserTest("UserOfDerivedWithStaticFooInBaseAndImplementedFooInInterface");
            invokeUserTest("UserOfDerivedWithStaticFooInBaseAndImplementedFooInInterface2");

            /*
             * Test invoke-interface SomeInterface.toString() which the RI resolves to
             * java.lang.Object.toString() and executes. ART used to resolve it in a secondary
             * resolution attempt only to distinguish between ICCE and NSME and then throw ICCE.
             * We now allow the call to proceed.
             *
             * We need to compile UserOfInterfaceToString with SomeInterface declaring toString()
             * but execute the test with SomeInterface that does not declare it. Files:
             *   src/UserOfInterfaceToString.java - calls invoke-interface SomeInterface.toString()
             *   src/SomeInterface.java           - declares toString(), not used in the test.
             *   src2/SomeInterface.java          - does not declare toString().
             */
            invokeUserTest("UserOfInterfaceToString");
        } catch (Throwable t) {
            t.printStackTrace(System.out);
        }
    }

    private static void invokeUserTest(String userName) throws Exception {
        System.out.println("Testing user class " + userName);
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
