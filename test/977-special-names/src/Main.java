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

import java.lang.reflect.Method;

public class Main {
  public static void main(String[] args) throws Exception {
      // Runtimes prior to 25 may log a class_linker warning, but do not
      // throw with the cases tested test.
      if (VMRuntime.getTargetSdkVersion(VMRuntime.getRuntime()) < 25) {
          VMRuntime.setTargetSdkVersion(VMRuntime.getRuntime(), 25);
      }

      // Tests for b/31313719.
      try {
          Class<?> c = Class.forName("BadName");
          Object o = c.newInstance();
      } catch (ClassFormatError e) {
          System.out.println("Reject bad static special name");
      }

      try {
          Class<?> c = Class.forName("BadVirtualName");
          Object o = c.newInstance();
      } catch (ClassFormatError e) {
          System.out.println("Reject bad virtual special name");
      }

      try {
          Class<?> c = Class.forName("OneClinitBadSig");
          Object o = c.newInstance();
      } catch (ClassFormatError e) {
          System.out.println("Reject bad clinit signature");
      }

      try {
          Class<?> c = Class.forName("TwoClinits");
          Object o = c.newInstance();
      } catch (ClassFormatError e) {
          System.out.println("Reject duplicate clinits");
      }

      try {
          Class<?> c = Class.forName("TwoClinitsStrikeBack");
          Object o = c.newInstance();
      } catch (ClassFormatError e) {
      } finally {
          // TODO: Fix/Configure smali not to remove duplicate <clinit>()V
          // or handcraft binary. And move this message into prior catch block.
          System.out.println("Reject duplicate clinits 2");
      }
  }

    private static class VMRuntime {
        private static Class<?> vmRuntimeClass;
        private static Method getRuntimeMethod;
        private static Method getTargetSdkVersionMethod;
        private static Method setTargetSdkVersionMethod;
        static {
            init();
        }

        private static void init() {
            try {
                vmRuntimeClass = Class.forName("dalvik.system.VMRuntime");
            } catch (Exception e) {
                return;
            }
            try {
                getRuntimeMethod = vmRuntimeClass.getDeclaredMethod("getRuntime");
                getTargetSdkVersionMethod =
                        vmRuntimeClass.getDeclaredMethod("getTargetSdkVersion");
                setTargetSdkVersionMethod =
                        vmRuntimeClass.getDeclaredMethod("setTargetSdkVersion", Integer.TYPE);
            } catch (Exception e) {
                throw new RuntimeException(e);
            }
        }

        public static boolean isAndroid() {
            return vmRuntimeClass != null;
        }

        public static Object getRuntime() throws Exception {
            return getRuntimeMethod.invoke(null);
        }

        public static int getTargetSdkVersion(Object runtime) throws Exception {
            return (int) getTargetSdkVersionMethod.invoke(runtime);
        }

        public static void setTargetSdkVersion(Object runtime, int version) throws Exception {
            setTargetSdkVersionMethod.invoke(runtime, version);
        }
    }
}
