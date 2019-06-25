/*
 * Copyright (C) 2019 The Android Open Source Project
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

import dalvik.system.VMRuntime;

import java.lang.ref.WeakReference;
import java.lang.reflect.Constructor;
import java.util.concurrent.atomic.AtomicBoolean;

public class Main {
    static final String DEX_FILE = System.getenv("DEX_LOCATION") + "/1002-notify-startup.jar";
    static final String LIBRARY_SEARCH_PATH = System.getProperty("java.library.path");
    static AtomicBoolean completed = new AtomicBoolean(false);

    public static void main(String[] args) {
        System.loadLibrary(args[0]);
        System.out.println("Startup completed: " + hasStartupCompleted());
        Thread workerThread = new WorkerThread();
        workerThread.start();
        while (!completed.get()) {
            resetStartupCompleted();
            VMRuntime.getRuntime().notifyStartupCompleted();
        }
        try {
            workerThread.join();
        } catch (Throwable e) {
            System.err.println(e);
        }
        System.out.println("Startup completed: " + hasStartupCompleted());
    }

    private static class WorkerThread extends Thread {
      Constructor<?> constructor;

      WorkerThread() {
          try {
              Class<?> pathClassLoader = Class.forName("dalvik.system.PathClassLoader");
              if (pathClassLoader == null) {
                  throw new AssertionError("Couldn't find path class loader class");
              }
              constructor = pathClassLoader.getDeclaredConstructor(
                      String.class, String.class, ClassLoader.class);
          } catch (Throwable e) {
              System.err.println(e);
          }
      }

      private WeakReference<Class<?>> loadClassInLoader() throws Exception {
          ClassLoader loader = (ClassLoader) constructor.newInstance(
                  DEX_FILE, LIBRARY_SEARCH_PATH, ClassLoader.getSystemClassLoader());
          Class ret = loader.loadClass("Main");
          return new WeakReference(ret);
      }

      public void run() {
          for (int i = 0; i < 100; ++i) {
              try {
                  WeakReference<Class<?>> ref = loadClassInLoader();
                  Runtime.getRuntime().gc();
                  // Don't validate the unloading since app images will keep classes live (for now).
              } catch (Throwable e) {
                  System.err.println(e);
                  break;
              }
          }
          completed.set(true);
      }
    }

    private static native boolean hasStartupCompleted();
    private static native void resetStartupCompleted();
}
