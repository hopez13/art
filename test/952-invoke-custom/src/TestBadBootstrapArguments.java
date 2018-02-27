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

import annotations.BootstrapMethod;
import annotations.CalledByIndy;
import annotations.Constant;
import java.lang.invoke.CallSite;
import java.lang.invoke.ConstantCallSite;
import java.lang.invoke.MethodHandle;
import java.lang.invoke.MethodHandles;
import java.lang.invoke.MethodType;
import java.lang.invoke.WrongMethodTypeException;

public class TestBadBootstrapArguments extends TestBase {
    private static CallSite bsm(
            MethodHandles.Lookup lookup,
            String methodName,
            MethodType methodType,
            int extraInt,
            String extraString)
            throws Throwable {
        System.out.print("bsm(");
        System.out.print(lookup.lookupClass());
        System.out.print(", ");
        System.out.print(methodName);
        System.out.print(", ");
        System.out.print(methodType);
        System.out.print(", ");
        System.out.print(extraInt);
        System.out.print(", ");
        System.out.print(extraString);
        System.out.println(")");
        MethodHandle mh = lookup.findStatic(lookup.lookupClass(), methodName, methodType);
        return new ConstantCallSite(mh);
    }

    @CalledByIndy(
        bootstrapMethod =
                @BootstrapMethod(
                    enclosingType = TestBadBootstrapArguments.class,
                    name = "bsm",
                    parameterTypes = {
                        MethodHandles.Lookup.class,
                        String.class,
                        MethodType.class,
                        int.class,
                        String.class
                    }
                ),
        fieldOrMethodName = "happy",
        constantArgumentsForBootstrapMethod = {
            @Constant(intValue = -1),
            @Constant(stringValue = "very")
        }
    )
    private static void invokeHappy() {
        assertNotReached();
    }

    private static void happy() {
        System.out.println("happy");
    }

    // BootstrapMethod.parameterTypes != parameterTypesOf(constantArgumentsForBootstrapMethod)
    @CalledByIndy(
        bootstrapMethod =
                @BootstrapMethod(
                    enclosingType = TestBadBootstrapArguments.class,
                    name = "bsm",
                    parameterTypes = {
                        MethodHandles.Lookup.class,
                        String.class,
                        MethodType.class,
                        int.class,
                        double.class
                    }
                ),
        fieldOrMethodName = "wrongParameterTypes",
        constantArgumentsForBootstrapMethod = {
            @Constant(intValue = -1),
            @Constant(stringValue = "very")
        }
    )
    private static void invokeWrongParameterTypes() throws NoSuchMethodError {
        assertNotReached();
    }

    private static void wrongParameterTypes() {
        System.out.println("wrongParameterTypes");
    }

    // BootstrapMethod.parameterTypes != parameterTypesOf(constantArgumentsForBootstrapMethod)
    // (missing constantArgumentTypes))
    @CalledByIndy(
        bootstrapMethod =
                @BootstrapMethod(
                    enclosingType = TestBadBootstrapArguments.class,
                    name = "bsm",
                    parameterTypes = {
                        MethodHandles.Lookup.class,
                        String.class,
                        MethodType.class,
                        int.class,
                        double.class
                    }
                ),
        fieldOrMethodName = "missingParameterTypes",
        constantArgumentsForBootstrapMethod = {}
    )
    private static void invokeMissingParameterTypes() throws NoSuchMethodError {
        assertNotReached();
    }

    private static void missingParameterTypes() {
        System.out.println("missingParameterTypes");
    }

    // BootstrapMethod.parameterTypes != parameterTypesOf(constantArgumentsForBootstrapMethod):
    // extra constant present
    @CalledByIndy(
        bootstrapMethod =
                @BootstrapMethod(
                    enclosingType = TestBadBootstrapArguments.class,
                    name = "bsm",
                    parameterTypes = {
                        MethodHandles.Lookup.class,
                        String.class,
                        MethodType.class,
                        int.class,
                        String.class
                    }
                ),
        fieldOrMethodName = "extraArguments",
        constantArgumentsForBootstrapMethod = {
            @Constant(intValue = 1),
            @Constant(stringValue = "2"),
            @Constant(intValue = 3)
        }
    )
    private static void invokeExtraArguments() {
        assertNotReached();
    }

    private static void extraArguments() {
        System.out.println("extraArguments");
    }

    // constantArgumentTypes do not correspond to expected parameter types
    @CalledByIndy(
        bootstrapMethod =
                @BootstrapMethod(
                    enclosingType = TestBadBootstrapArguments.class,
                    name = "bsm",
                    parameterTypes = {
                        MethodHandles.Lookup.class,
                        String.class,
                        MethodType.class,
                        int.class,
                        String.class
                    }
                ),
        fieldOrMethodName = "wrongArguments",
        constantArgumentsForBootstrapMethod = {
            @Constant(stringValue = "1"),
            @Constant(doubleValue = Math.PI)
        }
    )
    private static void invokeWrongArguments() {
        assertNotReached();
    }

    private static void wrongArguments() {
        System.out.println("wrongArguments");
    }

    // constantArgumentTypes do not correspond to expected parameter types
    @CalledByIndy(
        bootstrapMethod =
                @BootstrapMethod(
                    enclosingType = TestBadBootstrapArguments.class,
                    name = "bsm",
                    parameterTypes = {
                        MethodHandles.Lookup.class,
                        String.class,
                        MethodType.class,
                        int.class,
                        String.class
                    }
                ),
        fieldOrMethodName = "wrongArgumentsAgain",
        constantArgumentsForBootstrapMethod = {
            @Constant(doubleValue = Math.PI),
            @Constant(stringValue = "pie")
        }
    )
    private static void invokeWrongArgumentsAgain() {
        assertNotReached();
    }

    private static void wrongArgumentsAgain() {
        System.out.println("wrongArgumentsAgain");
    }

    static void test() {
        System.out.println("TestBadBootstrapArguments");
        invokeHappy();
        try {
            invokeWrongParameterTypes();
            assertNotReached();
        } catch (NoSuchMethodError expected) {
            System.out.print("invokeWrongParameterTypes => ");
            System.out.println(expected.getClass());
        }
        try {
            invokeMissingParameterTypes();
            assertNotReached();
        } catch (NoSuchMethodError expected) {
            System.out.print("invokeMissingParameterTypes => ");
            System.out.println(expected.getClass());
        }
        try {
            invokeExtraArguments();
            assertNotReached();
        } catch (BootstrapMethodError expected) {
            assertEquals(WrongMethodTypeException.class, expected.getCause().getClass());
            System.out.print("invokeExtraArguments => ");
            System.out.print(expected.getClass());
            System.out.print(" => ");
            System.out.println(expected.getCause().getClass());
        }
        try {
            invokeWrongArguments();
            assertNotReached();
        } catch (BootstrapMethodError expected) {
            assertEquals(ClassCastException.class, expected.getCause().getClass());
            System.out.print("invokeWrongArguments => ");
            System.out.print(expected.getClass());
            System.out.print(" => ");
            System.out.println(expected.getCause().getClass());
        }
        try {
            invokeWrongArguments();
            assertNotReached();
        } catch (BootstrapMethodError expected) {
            assertEquals(ClassCastException.class, expected.getCause().getClass());
            System.out.print("invokeWrongArguments => ");
            System.out.print(expected.getClass());
            System.out.print(" => ");
            System.out.println(expected.getCause().getClass());
        }
        try {
            invokeWrongArgumentsAgain();
            assertNotReached();
        } catch (BootstrapMethodError expected) {
            assertEquals(ClassCastException.class, expected.getCause().getClass());
            System.out.print("invokeWrongArgumentsAgain => ");
            System.out.print(expected.getClass());
            System.out.print(" => ");
            System.out.println(expected.getCause().getClass());
        }
    }
}
