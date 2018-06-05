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

import java.lang.reflect.Field;

public class Main {
    public static void main(String[] args) throws Exception {
        if (!isDalvik) {
          // This test is ART-specific. Just fake the expected output.
          System.out.println("JNI_OnLoad called");
          return;
        }
        System.loadLibrary(args[0]);
        if (!hasJit()) {
          return;
        }

        Class<?> integerCacheClass = Class.forName("java.lang.Integer$IntegerCache");
        Field cacheField = integerCacheClass.getDeclaredField("cache");
        cacheField.setAccessible(true);
        Field lowField = integerCacheClass.getDeclaredField("low");
        lowField.setAccessible(true);

        Object[] cache = (Object[]) cacheField.get(integerCacheClass);
        int low = (int) lowField.get(integerCacheClass);
        Object old42 = cache[42 - low];
        cache[42 - low] = new Integer(42);

        // This hits
        //     DCHECK(boxed != nullptr &&
        //            Runtime::Current()->GetHeap()->ObjectIsInBootImageSpace(boxed));
        // when compiling the intrinsic.
        ensureJitCompiled(Main.class, "get42AsInteger");

        cache[42 - low] = old42;
        Runtime.getRuntime().gc();
        Integer new42 = get42AsInteger();

        // If the DCHECK() is removed, MterpInvokeVirtualQuick() crashes here.
        // (Note: Our fault handler on x86-64 then also crashes.)
        int value = (int) new42;

        if (value != (int) old42) {
            throw new Error("value is " + value);
        }
    }

    public static Integer get42AsInteger() {
        return Integer.valueOf(42);
    }

    private native static boolean hasJit();
    private static native void ensureJitCompiled(Class<?> itf, String method_name);

    private final static boolean isDalvik = System.getProperty("java.vm.name").equals("Dalvik");
}
