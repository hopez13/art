/*
 * Copyright (C) 2016 The Android Open Source Project
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

import java.lang.ref.WeakReference;
import java.lang.reflect.Method;

public class Main {
    public static void main(String[] args) {
        System.loadLibrary(args[0]);
        try {
            ClassLoader system_loader = ClassLoader.getSystemClassLoader();
            WrapperLoader wrapper_loader = createLoader(system_loader);
            Class<?> helper = wrapper_loader.loadClass("Helper");
            Method get = helper.getDeclaredMethod("get");

            WeakReference<Class<?>> weak_test1 = wrapHelperGet(get);
            changeInner(wrapper_loader, system_loader);
            clearResolvedTypes(helper);
            Runtime.getRuntime().gc();
            WeakReference<Class<?>> weak_test2 = wrapHelperGet(get);

            Class<?> test1 = weak_test1.get();
            if (test1 == null) {
                System.out.println("test1 disappeared");
            }
            Class<?> test2 = weak_test2.get();
            if (test2 == null) {
                System.out.println("test2 disappeared");
            }
            if (test1 != test2) {
                System.out.println("test1 != test2");
            }
        } catch (Throwable t) {
            t.printStackTrace();
        }
    }

    private static WrapperLoader createLoader(ClassLoader system_loader) {
        FancyLoader fancy_loader = new FancyLoader(system_loader);
        return new WrapperLoader(system_loader, fancy_loader);
    }

    private static void changeInner(WrapperLoader wrapper_loader, ClassLoader system_loader) {
        FancyLoader fancy_loader = new FancyLoader(system_loader);
        wrapper_loader.resetFancyLoader(fancy_loader);
    }

    private static WeakReference<Class<?>> wrapHelperGet(Method get) throws Exception {
        return new WeakReference<Class<?>>((Class<?>) get.invoke(null));
    }

    public static native void clearResolvedTypes(Class<?> c);
}
