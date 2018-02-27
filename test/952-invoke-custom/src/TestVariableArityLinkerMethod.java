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

public class TestVariableArityLinkerMethod extends TestBase {
    private static CallSite bsmWithStringArray(
            MethodHandles.Lookup lookup,
            String methodName,
            MethodType methodType,
            String... arityArgs)
            throws Throwable {
        System.out.print("bsmWithStringArray(");
        System.out.print(lookup.lookupClass());
        System.out.print(", ");
        System.out.print(methodName);
        System.out.print(", ");
        System.out.print(methodType);
        for (String arg : arityArgs) {
            System.out.print(", ");
            System.out.print(arg);
        }
        System.out.println(")");
        MethodHandle mh = lookup.findStatic(lookup.lookupClass(), methodName, methodType);
        return new ConstantCallSite(mh);
    }

    @CalledByIndy(
        bootstrapMethod =
                @BootstrapMethod(
                    enclosingType = TestVariableArityLinkerMethod.class,
                    name = "bsmWithStringArray",
                    parameterTypes = {
                        MethodHandles.Lookup.class,
                        String.class,
                        MethodType.class,
                        String[].class
                    }
                ),
        fieldOrMethodName = "methodA",
        constantArgumentsForBootstrapMethod = {
            @Constant(stringValue = "Aachen"),
            @Constant(stringValue = "Aalborg"),
            @Constant(stringValue = "Aalto")
        }
    )
    private static void methodA() {
        System.out.println("methodA");
    }

    @CalledByIndy(
        bootstrapMethod =
                @BootstrapMethod(
                    enclosingType = TestVariableArityLinkerMethod.class,
                    name = "bsmWithStringArray",
                    parameterTypes = {
                        MethodHandles.Lookup.class,
                        String.class,
                        MethodType.class,
                        String[].class
                    }
                ),
        fieldOrMethodName = "methodB",
        constantArgumentsForBootstrapMethod = {@Constant(stringValue = "barium")}
    )
    private static void methodB() {
        System.out.println("methodB");
    }

    @CalledByIndy(
        bootstrapMethod =
                @BootstrapMethod(
                    enclosingType = TestVariableArityLinkerMethod.class,
                    name = "bsmWithStringArray",
                    parameterTypes = {
                        MethodHandles.Lookup.class,
                        String.class,
                        MethodType.class,
                        String[].class
                    }
                ),
        fieldOrMethodName = "methodC"
    )
    private static void methodC() {
        System.out.println("methodC");
    }

    private static CallSite bsmWithIntAndStringArray(
            MethodHandles.Lookup lookup,
            String methodName,
            MethodType methodType,
            int extraInt,
            String... extraArityArgs)
            throws Throwable {
        System.out.print("bsmWithIntAndStringArray(");
        System.out.print(lookup.lookupClass());
        System.out.print(", ");
        System.out.print(methodName);
        System.out.print(", ");
        System.out.print(methodType);
        System.out.print(", ");
        System.out.print(extraInt);
        for (String arg : extraArityArgs) {
            System.out.print(", ");
            System.out.print(arg);
        }
        System.out.println(")");
        MethodHandle mh = lookup.findStatic(lookup.lookupClass(), methodName, methodType);
        return new ConstantCallSite(mh);
    }

    @CalledByIndy(
        bootstrapMethod =
                @BootstrapMethod(
                    enclosingType = TestVariableArityLinkerMethod.class,
                    name = "bsmWithIntAndStringArray",
                    parameterTypes = {
                        MethodHandles.Lookup.class,
                        String.class,
                        MethodType.class,
                        int.class,
                        String[].class
                    }
                ),
        fieldOrMethodName = "methodD",
        constantArgumentsForBootstrapMethod = {
            @Constant(intValue = 101),
            @Constant(stringValue = "zoo"),
            @Constant(stringValue = "zoogene"),
            @Constant(stringValue = "zoogenic")
        }
    )
    private static void methodD() {
        System.out.println("methodD");
    }

    @CalledByIndy(
        bootstrapMethod =
                @BootstrapMethod(
                    enclosingType = TestVariableArityLinkerMethod.class,
                    name = "bsmWithIntAndStringArray",
                    parameterTypes = {
                        MethodHandles.Lookup.class,
                        String.class,
                        MethodType.class,
                        int.class,
                        String[].class
                    }
                ),
        fieldOrMethodName = "methodE",
        constantArgumentsForBootstrapMethod = {
            @Constant(intValue = 102),
            @Constant(stringValue = "zonic")
        }
    )
    private static void methodE() {
        System.out.println("methodE");
    }

    @CalledByIndy(
        bootstrapMethod =
                @BootstrapMethod(
                    enclosingType = TestVariableArityLinkerMethod.class,
                    name = "bsmWithIntAndStringArray",
                    parameterTypes = {
                        MethodHandles.Lookup.class,
                        String.class,
                        MethodType.class,
                        int.class,
                        String[].class
                    }
                ),
        fieldOrMethodName = "methodF",
        constantArgumentsForBootstrapMethod = {@Constant(intValue = 103)}
    )
    private static void methodF() {
        System.out.println("methodF");
    }

    @CalledByIndy(
        bootstrapMethod =
                @BootstrapMethod(
                    enclosingType = TestVariableArityLinkerMethod.class,
                    name = "bsmWithIntAndStringArray",
                    parameterTypes = {
                        MethodHandles.Lookup.class,
                        String.class,
                        MethodType.class,
                        int.class,
                        String[].class
                    }
                ),
        fieldOrMethodName = "methodO",
        constantArgumentsForBootstrapMethod = {@Constant(intValue = 103), @Constant(intValue = 104)}
    )
    private static void methodO() {
        // Arguments are not compatible
        assertNotReached();
    }

    @CalledByIndy(
        bootstrapMethod =
                @BootstrapMethod(
                    enclosingType = TestVariableArityLinkerMethod.class,
                    name = "bsmWithIntAndStringArray",
                    parameterTypes = {
                        MethodHandles.Lookup.class,
                        String.class,
                        MethodType.class,
                        int.class,
                        String[].class
                    }
                ),
        fieldOrMethodName = "methodP",
        constantArgumentsForBootstrapMethod = {
            @Constant(intValue = 103),
            @Constant(stringValue = "A"),
            @Constant(stringValue = "B"),
            @Constant(intValue = 42)
        }
    )
    private static void methodP() {
        // Arguments are not compatible - specifically, the third
        // component of potential collector array is an integer
        // argument (42).
        assertNotReached();
    }

    private static CallSite bsmWithWiderArray(
        MethodHandles.Lookup lookup,
        String methodName,
        MethodType methodType,
        long[] extraArgs) throws Throwable {
        MethodHandle mh = lookup.findStatic(lookup.lookupClass(), methodName, methodType);
        return new ConstantCallSite(mh);
    }

    @CalledByIndy(
        bootstrapMethod =
                @BootstrapMethod(
                    enclosingType = TestVariableArityLinkerMethod.class,
                    name = "bsmWithWiderArray",
                    parameterTypes = {
                        MethodHandles.Lookup.class,
                        String.class,
                        MethodType.class,
                        long[].class
                    }
                ),
        fieldOrMethodName = "methodQ",
        constantArgumentsForBootstrapMethod = {
            @Constant(intValue = 103),
            @Constant(intValue = 42)
        }
    )
    private static void methodQ() {
        assertNotReached();
    }

    private static CallSite bsmWithBoxedArray(
        MethodHandles.Lookup lookup,
        String methodName,
        MethodType methodType,
        Integer[] extraArgs) throws Throwable {
        MethodHandle mh = lookup.findStatic(lookup.lookupClass(), methodName, methodType);
        return new ConstantCallSite(mh);
    }

    @CalledByIndy(
        bootstrapMethod =
                @BootstrapMethod(
                    enclosingType = TestVariableArityLinkerMethod.class,
                    name = "bsmWithBoxedArray",
                    parameterTypes = {
                        MethodHandles.Lookup.class,
                        String.class,
                        MethodType.class,
                        Integer[].class
                    }
                ),
        fieldOrMethodName = "methodR",
        constantArgumentsForBootstrapMethod = {
            @Constant(intValue = 1030),
            @Constant(intValue = 420)
        }
    )
    private static void methodR() {
        assertNotReached();
    }

    static void test() {
        // Happy cases
        for (int i = 0; i < 2; ++i) {
            methodA();
            methodB();
            methodC();
        }
        for (int i = 0; i < 2; ++i) {
            methodD();
            methodE();
            methodF();
        }

        // Broken cases
        try {
            // bsm has incompatible static methods. Collector
            // component type is String, the corresponding static
            // arguments are int values.
            methodO();
            assertNotReached();
        } catch (BootstrapMethodError expected) {
            System.out.print("methodO => ");
            System.out.print(expected.getClass());
            System.out.print(" => ");
            System.out.println(expected.getCause().getClass());
        }
        try {
            // bsm has a trailing String array for the collector array.
            // There is an int value amongst the String values.
            methodP();
            assertNotReached();
        } catch (BootstrapMethodError expected) {
            System.out.print("methodP => ");
            System.out.print(expected.getClass());
            System.out.print(" => ");
            System.out.println(expected.getCause().getClass());
        }
        try {
            // bsm has as trailing long[] element for the collector array.
            // The corresponding static bsm arguments are of type int.
            methodQ();
            assertNotReached();
        } catch (BootstrapMethodError expected) {
            System.out.print("methodQ => ");
            System.out.print(expected.getClass());
            System.out.print(" => ");
            System.out.println(expected.getCause().getClass());
        }
        try {
            // bsm has as trailing Integer[] element for the collector array.
            // The corresponding static bsm arguments are of type int.
            methodR();
            assertNotReached();
        } catch (BootstrapMethodError expected) {
            System.out.print("methodR => ");
            System.out.print(expected.getClass());
            System.out.print(" => ");
            System.out.println(expected.getCause().getClass());
        }
    }
}
