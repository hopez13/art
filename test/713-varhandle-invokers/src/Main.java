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
    private static final VarHandle vhField;

    int field;

    static {
        try {
            vhField = MethodHandles.lookup().findVarHandle(Main.class, "field", int.class);
        } catch (Exception e) {
            throw new RuntimeException(e);
        }
    }

    private static void fail(String message) throws Throwable {
        StringBuilder sb = new StringBuilder();
        sb.append("Test failure: ");
        sb.append(message);
        throw new Exception(sb.toString());
    }

    private static void failUnreachable() throws Throwable {
        fail("Unreachable");
    }

    private static void assertEquals(int expected, int actual) throws Throwable {
        if (expected != actual) {
            StringBuilder sb = new StringBuilder();
            sb.append(expected);
            sb.append(" != ");
            sb.append(actual);
            fail(sb.toString());
        }
    }

    private void fieldVarHandleExactInvokerTest() throws Throwable {
        System.out.println("fieldVarHandleExactInvokerTest");

        MethodHandle mhField = MethodHandles.varHandleExactInvoker(
            VarHandle.AccessMode.GET_AND_SET,
            MethodType.methodType(int.class, Main.class, int.class));

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
    }

    private void fieldVarHandleInvokerTest() throws Throwable {
        System.out.println("fieldVarHandleInvokerTest");

        MethodHandle mhField = MethodHandles.varHandleInvoker(
            VarHandle.AccessMode.GET_AND_SET,
            MethodType.methodType(int.class, Main.class, int.class));

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
    }

    public static void main(String[] args) throws Throwable {
        new Main().fieldVarHandleExactInvokerTest();
        new Main().fieldVarHandleInvokerTest();
    }
}
