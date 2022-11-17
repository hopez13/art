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

import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.lang.reflect.Modifier;
import java.util.LinkedList;
import java.util.List;

/**
 * Smali exercise, copied from 800-smali and modified for this test case.
 */
public class Main {

    private static class TestCase {
        public TestCase(String testName, String testClass, String testMethodName, Object[] values,
                        Object expectedReturn) {
            this.testName = testName;
            this.testClass = testClass;
            this.testMethodName = testMethodName;
            this.values = values;
            this.expectedReturn = expectedReturn;
        }

        String testName;
        String testClass;
        String testMethodName;
        Object[] values;
        Object expectedReturn;
    }

    private List<TestCase> testCases;

    public Main() {
        // Create the test cases.
        testCases = new LinkedList<TestCase>();
        testCases.add(new TestCase("b/252804549 (instance-of)", "B252804549",
                "compareInstanceOfWithTwo", new Object[] {new Object()}, 3));
    }

    public void runTests() {
        for (TestCase tc : testCases) {
            System.out.println(tc.testName);
            try {
                runTest(tc);
            } catch (Exception exc) {
                exc.printStackTrace(System.out);
            }
        }
    }

    private void runTest(TestCase tc) throws Exception {
        Exception errorReturn = null;
        try {
            Class<?> c = Class.forName(tc.testClass);

            Method[] methods = c.getDeclaredMethods();

            // For simplicity we assume that test methods are not overloaded. So searching by name
            // will give us the method we need to run.
            Method method = null;
            for (Method m : methods) {
                if (m.getName().equals(tc.testMethodName)) {
                    method = m;
                    break;
                }
            }

            if (method == null) {
                errorReturn = new IllegalArgumentException("Could not find test method " +
                                                           tc.testMethodName + " in class " +
                                                           tc.testClass + " for test " +
                                                           tc.testName);
            } else {
                Object retValue;
                if (Modifier.isStatic(method.getModifiers())) {
                    retValue = method.invoke(null, tc.values);
                } else {
                    retValue = method.invoke(method.getDeclaringClass().newInstance(), tc.values);
                }
                if (tc.expectedReturn == null && retValue != null) {
                    errorReturn = new IllegalStateException("Expected a null result in test " +
                                                            tc.testName + " got " + retValue);
                } else if (tc.expectedReturn != null &&
                           (retValue == null || !tc.expectedReturn.equals(retValue))) {
                    errorReturn = new IllegalStateException("Expected return " +
                                                            tc.expectedReturn +
                                                            ", but got " + retValue);
                } else if (compiledWithOptimizing() && !isAotVerified(c)) {
                    errorReturn = new IllegalStateException("Expected method " + method.getName() +
                            " of class " + c.getName() +
                            " be verified in compile-time in test " + tc.testName);
                } else {
                    // Expected result, do nothing.
                }
            }
        } finally {
            if (errorReturn != null) {
                throw errorReturn;
            }
        }
    }

    public static void main(String[] args) throws Exception {
        System.loadLibrary(args[0]);
        Main main = new Main();
        main.runTests();
        System.out.println("Done!");
    }

    private native static boolean isAotVerified(Class<?> cls);
    private native static boolean compiledWithOptimizing();
}
