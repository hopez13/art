## -*- coding: utf-8 -*-
##
## Copyright (C) 2016 The Android Open Source Project
##
##  Licensed under the Apache License, Version 2.0 (the "License");
##  you may not use this file except in compliance with the License.
##  You may obtain a copy of the License at
##
##       http://www.apache.org/licenses/LICENSE-2.0
##
##  Unless required by applicable law or agreed to in writing, software
##  distributed under the License is distributed on an "AS IS" BASIS,
##  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
##  See the License for the specific language governing permissions and
##  limitations under the License.
##
/*
 * Copyright (C) 2007 The Android Open Source Project
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

/*
 * AUTOMATICALLY GENERATED FROM art/tools/mako-source-generator/...../NativeTestTarget.java.mako
 *
 * !!! DO NOT EDIT DIRECTLY !!!
 *
 */
package org.apache.harmony.dalvik;

import dalvik.annotation.optimization.CriticalNative;
import dalvik.annotation.optimization.FastNative;

/**
 * Methods used to test calling into native code. The methods in this
 * class are all effectively no-ops and may be used to test the mechanisms
 * and performance of calling native methods.
 */
public final class NativeTestTarget {
    public NativeTestTarget() {
    }

    /**
     * This is used to benchmark dalvik's inline natives.
     */
    public static void emptyInlineMethod() {
    }

    /**
     * This is used to benchmark dalvik's inline natives.
     */
    public static native void emptyInternalStaticMethod();

    // Synchronized methods. Test normal JNI only.
    public static native synchronized void emptyJniStaticSynchronizedMethod0();
    public native synchronized void emptyJniSynchronizedMethod0();

    // Static methods without object parameters. Test all optimization combinations.
% for annotation, jni_suffix in [("// Normal native", ""), ("@FastNative", "_Fast"), ("@CriticalNative", "_Critical")]:

    ${annotation}
    public static native void emptyJniStaticMethod0${jni_suffix}();
    ${annotation}
    public static native void emptyJniStaticMethod6${jni_suffix}(int a, int b, int c, int d, int e, int f);
% endfor
    // Instance methods or methods with object parameters. Test {Normal, @FastNative} combinations.
% for annotation, jni_suffix in [("// Normal native", ""), ("@FastNative", "_Fast")]:

    ${annotation}
    public native void emptyJniMethod0${jni_suffix}();
    ${annotation}
    public native void emptyJniMethod6${jni_suffix}(int a, int b, int c, int d, int e, int f);

    /**
     * This is an empty native static method with six args, hooked up
     * using JNI. These have more complex args to show the cost of
     * parsing the signature. All six values should be null
     * references.
     */
    ${annotation}
    public static native void emptyJniStaticMethod6L${jni_suffix}(String a, String[] b,
        int[][] c, Object d, Object[] e, Object[][][][] f);

    ${annotation}
    public native void emptyJniMethod6L${jni_suffix}(String a, String[] b,
        int[][] c, Object d, Object[] e, Object[][][][] f);
% endfor
}
