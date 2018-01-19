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

import dalvik.system.PathClassLoader;
import java.io.File;

public class Main {
  public static void main(String[] args) throws Exception {
    ClassLoader c1 = new PathClassLoader(DEX, ClassLoader.getSystemClassLoader());
    if (!canFindMyClass(c1)) {
      System.out.println("Could not find MyClass in normal dex file");
    }

    ClassLoader c2 =
        new PathClassLoader(DEX_WITH_HIDDENAPI_FLAGS, ClassLoader.getSystemClassLoader());
    if (canFindMyClass(c2)) {
      System.out.println("Should not be able find MyClass in dex file with hiddenapi flags");
    }
  }

  private static boolean canFindMyClass(ClassLoader c) {
    try {
      Class.forName("MyClass", true, c);
      return true;
    } catch (ClassNotFoundException ex) {
      return false;
    }
  }

  private static final String DEX =
      new File(System.getenv("DEX_LOCATION"), "675-hiddenapi-verifier.jar").getAbsolutePath();
  private static final String DEX_WITH_HIDDENAPI_FLAGS =
      new File(new File(System.getenv("DEX_LOCATION"), "res"), "boot.jar").getAbsolutePath();
}
