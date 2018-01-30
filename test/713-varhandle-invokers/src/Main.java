/*
 * Copyright (C) 2018 The Android Open Source Project
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

import java.lang.invoke.MethodHandle;
import java.lang.invoke.MethodHandles;
import java.lang.invoke.MethodType;
import java.lang.invoke.VarHandle;
import java.lang.invoke.WrongMethodTypeException;

public final class Main {
    private static void fail(String message) throws Throwable {
        StringBuilder sb = new StringBuilder();
        sb.append("Test failure: ");
        sb.append(message);
        throw new Exception(sb.toString());
    }

    private static void failUnreachable() throws Throwable {
        fail("Unreachable");
    }

    private static void failAssertEquals(Object expected, Object actual) throws Throwable {
        StringBuilder sb = new StringBuilder();
        sb.append(expected);
        sb.append(" != ");
        sb.append(actual);
        fail(sb.toString());
    }

    private static void assertEquals(int expected, int actual) throws Throwable {
        if (expected != actual) {
            failAssertEquals(expected, actual);
        }
    }

    private static void assertEquals(float expected, float actual) throws Throwable {
        if (expected != actual) {
            failAssertEquals(expected, actual);
        }
    }

    private static void assertEquals(double expected, double actual) throws Throwable {
        if (expected != actual) {
            failAssertEquals(expected, actual);
        }
    }

    static class FieldVarHandleExactInvokerTest {
        private static final VarHandle vhField;
        int field;

        static {
            try {
                vhField =
                        MethodHandles.lookup()
                                .findVarHandle(
                                        FieldVarHandleExactInvokerTest.class, "field", int.class);
            } catch (Exception e) {
                throw new RuntimeException(e);
            }
        }

        void run() throws Throwable {
            System.out.println("fieldVarHandleExactInvokerTest");

            MethodHandle mhField =
                    MethodHandles.varHandleExactInvoker(
                            VarHandle.AccessMode.GET_AND_SET,
                            MethodType.methodType(
                                    int.class, FieldVarHandleExactInvokerTest.class, int.class));

            field = 3;
            assertEquals(3, (int) mhField.invokeExact(vhField, this, 4));
            assertEquals(4, field);

            //
            // Check invocations with MethodHandle.invokeExact()
            //
            try {
                // Check for unboxing
                int dummy = (int) mhField.invokeExact(vhField, this, Integer.valueOf(3));
                fail("Unreachable");
            } catch (WrongMethodTypeException e) {
                assertEquals(4, field);
            }
            try {
                // Check for widening conversion
                int dummy = (int) mhField.invokeExact(vhField, this, (short) 3);
                fail("Unreachable");
            } catch (WrongMethodTypeException e) {
                assertEquals(4, field);
            }
            try {
                // Check for acceptance of void return type
                mhField.invokeExact(vhField, this, 77);
                fail("Unreachable");
            } catch (WrongMethodTypeException e) {
                assertEquals(4, field);
            }
            try {
                // Check for wider return type
                long dummy = (long) mhField.invokeExact(vhField, this, 77);
                fail("Unreachable");
            } catch (WrongMethodTypeException e) {
                assertEquals(4, field);
            }
            try {
                VarHandle vhNull = null;
                int x = (int) mhField.invokeExact(vhNull, this, 777);
                fail("Unreachable");
            } catch (NullPointerException e) {
                assertEquals(4, field);
            }

            //
            // Check invocations with MethodHandle.invoke()
            //
            try {
                // Check for unboxing
                int dummy = (int) mhField.invoke(vhField, this, Integer.valueOf(3));
                assertEquals(3, field);
            } catch (WrongMethodTypeException e) {
                fail("Unreachable");
            }
            try {
                // Check for unboxing
                int dummy = (int) mhField.invoke(vhField, this, Short.valueOf((short) 4));
                assertEquals(4, field);
            } catch (WrongMethodTypeException e) {
                fail("Unreachable");
            }
            try {
                // Check for widening conversion
                int dummy = (int) mhField.invoke(vhField, this, (short) 23);
                assertEquals(23, field);
            } catch (WrongMethodTypeException e) {
                fail("Unreachable");
            }
            try {
                // Check for acceptance of void return type
                mhField.invoke(vhField, this, 77);
                assertEquals(77, field);
            } catch (WrongMethodTypeException e) {
                fail("Unreachable");
            }
            try {
                // Check for wider return type
                long dummy = (long) mhField.invoke(vhField, this, 88);
                assertEquals(88, field);
            } catch (WrongMethodTypeException e) {
                fail("Unreachable");
            }
            try {
                VarHandle vhNull = null;
                int x = (int) mhField.invoke(vhNull, this, 888);
                fail("Unreachable");
            } catch (NullPointerException e) {
                assertEquals(88, field);
            }
        }
    }

    static class FieldVarHandleInvokerTest {
        private static final VarHandle vhField;
        int field;

        static {
            try {
                vhField =
                        MethodHandles.lookup()
                                .findVarHandle(FieldVarHandleInvokerTest.class, "field", int.class);
            } catch (Exception e) {
                throw new RuntimeException(e);
            }
        }

        void run() throws Throwable {
            System.out.println("fieldVarHandleInvokerTest");

            MethodHandle mhField =
                    MethodHandles.varHandleInvoker(
                            VarHandle.AccessMode.GET_AND_SET,
                            MethodType.methodType(
                                    int.class, FieldVarHandleInvokerTest.class, int.class));

            field = 3;
            int oldField = (int) mhField.invoke(vhField, this, 4);
            assertEquals(3, oldField);
            assertEquals(4, field);

            //
            // Check invocations with MethodHandle.invoke()
            //
            try {
                // Check for unboxing
                int dummy = (int) mhField.invoke(vhField, this, Integer.valueOf(3));
                assertEquals(3, field);
            } catch (WrongMethodTypeException e) {
                fail("Unreachable");
            }
            try {
                // Check for widening conversion
                int dummy = (int) mhField.invoke(vhField, this, (short) 33);
                assertEquals(33, field);
            } catch (WrongMethodTypeException e) {
                fail("Unreachable");
            }
            try {
                // Check for widening conversion
                int dummy = (int) mhField.invoke(vhField, this, Byte.valueOf((byte) 34));
                assertEquals(34, field);
            } catch (WrongMethodTypeException e) {
                fail("Unreachable");
            }
            try {
                // Check for acceptance of void return type
                mhField.invoke(vhField, this, 77);
                assertEquals(77, field);
            } catch (WrongMethodTypeException e) {
                fail("Unreachable");
            }
            try {
                // Check for wider return type
                long dummy = (long) mhField.invoke(vhField, this, 88);
                assertEquals(88, field);
            } catch (WrongMethodTypeException e) {
                fail("Unreachable");
            }
            try {
                VarHandle vhNull = null;
                int x = (int) mhField.invoke(vhNull, this, 888);
                fail("Unreachable");
            } catch (NullPointerException e) {
                assertEquals(88, field);
            }

            //
            // Check invocations with MethodHandle.invokeExact()
            //
            field = -1;
            try {
                // Check for unboxing
                int dummy = (int) mhField.invokeExact(vhField, this, Integer.valueOf(3));
                fail("Unreachable");
            } catch (WrongMethodTypeException e) {
                assertEquals(-1, field);
            }
            try {
                // Check for widening conversion
                int dummy = (int) mhField.invokeExact(vhField, this, (short) 33);
                fail("Unreachable");
            } catch (WrongMethodTypeException e) {
                assertEquals(-1, field);
            }
            try {
                // Check for acceptance of void return type
                mhField.invokeExact(vhField, this, 77);
                fail("Unreachable");
            } catch (WrongMethodTypeException e) {
                assertEquals(-1, field);
            }
            try {
                // Check for wider return type
                long dummy = (long) mhField.invokeExact(vhField, this, 78);
                fail("Unreachable");
            } catch (WrongMethodTypeException e) {
                assertEquals(-1, field);
            }
            try {
                VarHandle vhNull = null;
                int x = (int) mhField.invokeExact(vhNull, this, 888);
                fail("Unreachable");
            } catch (NullPointerException e) {
                assertEquals(-1, field);
            }
        }
    }

    static class DivergenceInexactTest {
        private static final VarHandle vhArray;

        static {
            try {
                vhArray = MethodHandles.arrayElementVarHandle(float[].class);
            } catch (Exception e) {
                throw new RuntimeException(e);
            }
        }

        void run() throws Throwable {
            System.out.println("DivergenceInexactTest");
            float[] array = new float[4];
            MethodHandle vhInvoker =
                    MethodHandles.varHandleInvoker(
                            VarHandle.AccessMode.COMPARE_AND_EXCHANGE,
                            MethodType.methodType(
                                    float.class,
                                    float[].class,
                                    int.class,
                                    Float.class,
                                    float.class));
            array[2] = Float.valueOf(4.0f);
            // Callsite that is an exact match with vhInvoker.type()
            float old = (float) vhInvoker.invoke(vhArray, array, 2, Float.valueOf(4.0f), 8.0f);
            assertEquals(4.0f, old);
            assertEquals(8.0f, array[2]);

            // Callsite that is convertible match to vhInvoker.type()
            old = (float) vhInvoker.invoke(vhArray, array, 2, 8.0f, 16.0f);
            assertEquals(8.0f, old);
            assertEquals(16.0f, array[2]);

            // Callsites that are not convertible to vhInvoker.type().
            try {
                old = (float) vhInvoker.invoke(vhArray, array, 2, (short) 4, 8.0f);
                fail("Unreachable");
            } catch (WrongMethodTypeException e) {
                assertEquals(16.0f, array[2]);
            }
            try {
                old = (float) vhInvoker.invoke(vhArray, array, 2, 8, -8.0f);
                fail("Unreachable");
            } catch (WrongMethodTypeException e) {
                assertEquals(16.0f, array[2]);
            }
        }
    }

    static class DivergenceExactTest {
        private static final VarHandle vhArray;

        static {
            try {
                vhArray = MethodHandles.arrayElementVarHandle(float[].class);
            } catch (Exception e) {
                throw new RuntimeException(e);
            }
        }

        void run() throws Throwable {
            System.out.println("DivergenceExactTest");
            float[] array = new float[4];
            MethodHandle vhInvoker =
                    MethodHandles.varHandleExactInvoker(
                            VarHandle.AccessMode.COMPARE_AND_EXCHANGE,
                            MethodType.methodType(
                                    float.class,
                                    float[].class,
                                    int.class,
                                    Float.class,
                                    float.class));
            array[2] = Float.valueOf(4.0f);
            // Callsite that is an exact match with vhInvoker.type(),
            // but vhInvokerType is not an exact match with the VarHandle accessor.
            try {
                float old = (float) vhInvoker.invoke(vhArray, array, 2, Float.valueOf(4.0f), 8.0f);
                assertEquals(4.0f, old);
            } catch (WrongMethodTypeException e) {
                assertEquals(4.0f, array[2]);
            }

            // Callsites that are exact matches with vhInvoker.type()
            try {
                float old = (float) vhInvoker.invoke(vhArray, array, 2, 8.0f, 16.0f);
                fail("Unreachable");
            } catch (WrongMethodTypeException e) {
                assertEquals(4.0f, array[2]);
            }
            try {
                float old = (float) vhInvoker.invoke(vhArray, array, 2, (short) 4, 13.0f);
                fail("Unreachable");
            } catch (WrongMethodTypeException e) {
                assertEquals(4.0f, array[2]);
            }
            try {
                float old = (float) vhInvoker.invoke(vhArray, array, 2, 8, -8.0f);
                fail("Unreachable");
            } catch (WrongMethodTypeException e) {
                assertEquals(4.0f, array[2]);
            }
        }
    }

    public static void main(String[] args) throws Throwable {
        new FieldVarHandleExactInvokerTest().run();
        new FieldVarHandleInvokerTest().run();
        new DivergenceInexactTest().run();
        new DivergenceExactTest().run();
    }
}
