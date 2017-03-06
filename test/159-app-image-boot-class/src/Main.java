/*
 * Copyright 2017 The Android Open Source Project
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

import java.lang.reflect.Constructor;
import java.lang.reflect.Method;

class Main {
    static class ExtendsBootClass extends VerifyError {
        public static int abc = 0;
    }

    static Object o;
    public static void main(String[] args) {
        try {
            System.loadLibrary(args[0]);
            System.out.println("Is boot image class: " + bootImageContains(VerifyError.class));
            o = new ExtendsBootClass();
            Class klass = loadSelf();
            Method runTest = klass.getDeclaredMethod("runTest", String.class, Class.class);
            runTest.invoke(klass, args[0], o.getClass().getSuperclass());
        } catch (Exception e) {
            System.out.println(e);
        }
    }
    
    public static void runTest(String arg, Class parentVersion) {
        System.loadLibrary(arg);
        try {
            Class classLoaderVersion = Class.forName("java.lang.VerifyError");
            Class superVersion = new ExtendsBootClass().getClass().getSuperclass();
            Class directVersion = VerifyError.class;
            System.out.println("Classes match: " + (classLoaderVersion == superVersion));
            System.out.println("Classes match: " + (superVersion == directVersion));
            // Should be in the boot class loader
            System.out.println("Is boot class loader class: " +
                    (classLoaderVersion.getClassLoader() == String.class.getClassLoader()));
            // Should not be in the boot image.
            System.out.println("Is boot image class: " + bootImageContains(classLoaderVersion));
            // Should not be in the app image.
            System.out.println("Same as parent class: " + (classLoaderVersion == parentVersion));
            // Extender should be in app image.
            System.out.println("Extends is app image class: " +
                    appImageContains(ExtendsBootClass.class));
        } catch (Exception e) {
            System.out.println(e);
        }
    }
    
    static final String DEX_FILE = System.getenv("DEX_LOCATION") + "/159-app-image-boot-class.jar";
    static final String LIBRARY_SEARCH_PATH = System.getProperty("java.library.path");

    public static Class loadSelf() throws Exception {
        Class<?> loaderClass = Class.forName("dalvik.system.PathClassLoader");
        Constructor<?> constructor =
                loaderClass.getDeclaredConstructor(String.class, String.class, ClassLoader.class);
        ClassLoader loader = (ClassLoader) constructor.newInstance(
                DEX_FILE, LIBRARY_SEARCH_PATH, ClassLoader.getSystemClassLoader());
        return loader.loadClass("Main");
    }

    public static native boolean bootImageContains(Object klass);
    public static native boolean appImageContains(Object klass);
}
