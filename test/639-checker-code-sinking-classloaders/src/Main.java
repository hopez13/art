/*
 * Copyright (C) 2017 The Android Open Source Project
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

import dalvik.system.DexClassLoader;
import java.io.File;
import java.lang.reflect.Method;

public class Main {

  public static void main(String[] args) throws Exception {
    testUserDefinedClassLoader();
  }

  static class UserDefinedClassLoader extends DexClassLoader {
    // User defined class loader constants
    private static final String CLASS_PATH = System.getenv("DEX_LOCATION") + "/639-checker-code-sinking-classloaders-ex.jar";
    private static final String ODEX_DIR = System.getenv("DEX_LOCATION");
    private static final String ODEX_ALT = "/tmp";
    private static final String LIB_DIR = "/nowhere/nothing/";

    private static final String getOdexDir() {
        return new File(ODEX_DIR).isDirectory() ? ODEX_DIR : ODEX_ALT;
    }

    public UserDefinedClassLoader(ClassLoader parent) {
      super(CLASS_PATH, getOdexDir(), LIB_DIR, parent);
    }

    @Override
    public Class<?> loadClass(String name) throws ClassNotFoundException {
      System.out.println(name);
      return super.loadClass(name);
    }
  }

  public static void testUserDefinedClassLoader() throws Exception {
    ClassLoader udc = new UserDefinedClassLoader(/* parent */ Main.class.getClassLoader());
    Class<?> exMainClass = udc.loadClass("ExMain");
    Method exMainTestMethod = exMainClass.getMethod("doTest");
    exMainTestMethod.invoke(null);
  }
}
