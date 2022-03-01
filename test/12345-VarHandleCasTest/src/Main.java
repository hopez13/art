/*
 * Copyright (C) 2021 The Android Open Source Project
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

import java.lang.invoke.VarHandle;
import java.lang.invoke.MethodHandles;
import java.util.concurrent.atomic.AtomicInteger;
import java.util.function.Consumer;

public class Main {
  private static final VarHandle QA;
  static {
      QA = MethodHandles.arrayElementVarHandle(TestTask[].class);
  }

  private static final int TASK_COUNT = 10000;
  private static final int THREAD_COUNT = 100;
  private static final int RETRIES = 500;

  private static class TestThread extends Thread {

      private final TestTask[] tasks;

      TestThread(TestTask[] tasks) {
          this.tasks = tasks;
      }

      @Override
      public void run() {
          for (int i = 0; i < TASK_COUNT; ++i) {
              TestTask t = (TestTask)QA.get(tasks, i);
              if (t == null) {
                  continue;
              }
              if (!QA.compareAndSet(tasks, i, t, null)) {
                  continue;
              }
              VarHandle.releaseFence();
              t.exec();
          }
      }
  }

  private static class TestTask {
      private final Integer ord;
      private final Consumer<Integer> action;

      TestTask(Integer ord, Consumer<Integer> action) {
          this.ord = ord;
          this.action = action;
      }

      public void exec() {
          action.accept(ord);
      }
  }

  private static void check(int actual, int expected, String msg, int iteration) {
    if (actual != expected) {
      System.out.println("[iteration " + iteration + "] " + msg + " : " + actual + " != " + expected);
      System.exit(1);
    }
  }

  public static void main(String[] args) throws Throwable {
    System.loadLibrary(args[0]);

    for (int i = 0; i < RETRIES; ++i) {
      testConcurrentProcessing(i);
    }
  }

  private static void testConcurrentProcessing(int iteration) throws Throwable {
      final TestTask[] tasks = new TestTask[TASK_COUNT];
      final AtomicInteger result = new AtomicInteger();

      for (int i = 0; i < TASK_COUNT; ++i) {
          tasks[i] = new TestTask(Integer.valueOf(i+1), result::addAndGet);
      }

      TestThread[] threads = new TestThread[THREAD_COUNT];
      for (int i = 0; i < THREAD_COUNT; ++i) {
          threads[i] = new TestThread(tasks);
      }

      for (int i = 0; i < THREAD_COUNT; ++i) {
          threads[i].start();
      }

      for (int i = 0; i < THREAD_COUNT; ++i) {
          threads[i].join();
      }

      check(result.get(), (TASK_COUNT >> 1) * (TASK_COUNT + 1),
              "Result not as expected", iteration);
  }

}
