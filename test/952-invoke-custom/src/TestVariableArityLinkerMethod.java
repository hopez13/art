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
    private static CallSite linkerMethod(
            MethodHandles.Lookup lookup,
            String methodName,
            MethodType methodType,
            String... arityArgs)
            throws Throwable {
        System.out.print("linkerMethod(");
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
                    name = "linkerMethod",
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
                    name = "linkerMethod",
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
                    name = "linkerMethod",
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

    private static CallSite linkerMethod2(
            MethodHandles.Lookup lookup,
            String methodName,
            MethodType methodType,
            int extraInt,
            String... extraArityArgs)
            throws Throwable {
        System.out.print("linkerMethod2(");
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
                    name = "linkerMethod2",
                    parameterTypes = {
                        MethodHandles.Lookup.class,
                        String.class,
                        MethodType.class,
                        int.class,
                        String[].class
                    }
                ),
        fieldOrMethodName = "methodX",
        constantArgumentsForBootstrapMethod = {
            @Constant(intValue = 101),
            @Constant(stringValue = "zoo"),
            @Constant(stringValue = "zoogene"),
            @Constant(stringValue = "zoogenic")
        }
    )
    private static void methodX() {
        System.out.println("methodX");
    }

    @CalledByIndy(
        bootstrapMethod =
                @BootstrapMethod(
                    enclosingType = TestVariableArityLinkerMethod.class,
                    name = "linkerMethod2",
                    parameterTypes = {
                        MethodHandles.Lookup.class,
                        String.class,
                        MethodType.class,
                        int.class,
                        String[].class
                    }
                ),
        fieldOrMethodName = "methodY",
        constantArgumentsForBootstrapMethod = {
            @Constant(intValue = 102),
            @Constant(stringValue = "zonic")
        }
    )
    private static void methodY() {
        System.out.println("methodY");
    }

    @CalledByIndy(
        bootstrapMethod =
                @BootstrapMethod(
                    enclosingType = TestVariableArityLinkerMethod.class,
                    name = "linkerMethod2",
                    parameterTypes = {
                        MethodHandles.Lookup.class,
                        String.class,
                        MethodType.class,
                        int.class,
                        String[].class
                    }
                ),
        fieldOrMethodName = "methodZ",
        constantArgumentsForBootstrapMethod = {@Constant(intValue = 103)}
    )
    private static void methodZ() {
        System.out.println("methodZ");
    }

    static void test() {
        // These linkerMethod() which has just a variable arity
        // string array for additional linker arguments.
        for (int i = 0; i < 2; ++i) {
            methodA();
            methodB();
            methodC();
        }

        for (int i = 0; i < 2; ++i) {
            // These linkerMethod2() which has an integer argument and
            // a variable arity string array for additional linker
            // arguments.
            methodX();
            methodY();
            methodZ();
        }
    }
}
