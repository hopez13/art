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

import sun.misc.SharedSecrets;

class Main {
    static class ExtendsBootClass extends sun.misc.SharedSecrets {
        public static int abc = 0;
    }

    public static void main(String[] args) {
        System.loadLibrary(args[0]);
        try {
            Class classLoaderVersion = Class.forName("sun.misc.SharedSecrets");
            Class superVersion = new ExtendsBootClass().getClass().getSuperclass();
            Class directVersion = sun.misc.SharedSecrets.class;
            System.out.println("Classes match: " + (classLoaderVersion == superVersion));
            System.out.println("Classes match: " + (superVersion == directVersion));
            // Should be in the boot class loader
            System.out.println("Is boot class loader class: " +
                  (classLoaderVersion.getClassLoader() == String.class.getClassLoader()));
            // Should not be in the boot image.
            System.out.println("Is boot image class: " + bootImageContains(classLoaderVersion));
            // Should not be in the app image.
            System.out.println("Is app image class: " + appImageContains(classLoaderVersion));
            // Extender should be in app image.
            System.out.println("Extends is app image class: " +
                  appImageContains(ExtendsBootClass.class));
        } catch (Exception e) {
            System.out.println(e);
        }
    }

    public static native boolean bootImageContains(Object klass);
    public static native boolean appImageContains(Object klass);
}
