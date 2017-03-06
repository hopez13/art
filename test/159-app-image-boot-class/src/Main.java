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
            System.out.println(classLoaderVersion == superVersion);
            System.out.println(superVersion == directVersion);
        } catch (Exception e) {
            System.out.println(e);
        }
      /*
      if (!checkAppImageLoaded()) {
        System.out.println("App image is not loaded!");
      } else if (!checkAppImageContains(Inner.class)) {
        System.out.println("App image does not contain Inner!");
      }*/
    }

    public static native boolean checkAppImageLoaded();
    public static native boolean checkAppImageContains(Class<?> klass);
}
