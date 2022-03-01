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

  private static abstract class TaskRunner extends Thread {

      protected final TestTask[] tasks;

      TaskRunner(TestTask[] tasks) {
          this.tasks = tasks;
      }

      @Override
      public void run() {
          int i = 0;
          while (i < TASK_COUNT) {
              TestTask t = (TestTask)QA.get(tasks, i);
              if (t == null) {
                  ++i;
                  continue;
              }
              if (!grabTask(t, i)) {
                  continue;
              }
              ++i;
              VarHandle.releaseFence();
              t.exec();
          }
      }

      protected abstract boolean grabTask(TestTask t, int i);

  }

  private static class TaskRunnerWithCompareAndSet extends TaskRunner {


      TaskRunnerWithCompareAndSet(TestTask[] tasks) {
          super(tasks);
      }

      @Override
      protected boolean grabTask(TestTask t, int i) {
          return QA.compareAndSet(tasks, i, t, null);
      }
  }

  private static class TaskRunnerWithCompareAndExchange extends TaskRunner {

      TaskRunnerWithCompareAndExchange(TestTask[] tasks) {
          super(tasks);
      }

      @Override
      protected boolean grabTask(TestTask t, int i) {
          return (t == QA.compareAndExchange(tasks, i, t, null));
      }
  }

  private interface RunnerFactory {
      Thread createRunner(TestTask[] tasks);
  }

  private static class CompareAndSetRunnerFactory implements RunnerFactory {
      @Override
      public Thread createRunner(TestTask[] tasks) {
          return new TaskRunnerWithCompareAndSet(tasks);
      }
  }

  private static class CompareAndExchangeRunnerFactory implements RunnerFactory {
      @Override
      public Thread createRunner(TestTask[] tasks) {
          return new TaskRunnerWithCompareAndExchange(tasks);
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
      testConcurrentProcessing(new CompareAndExchangeRunnerFactory(), i, "compareAndExchange");
    }
    for (int i = 0; i < RETRIES; ++i) {
      testConcurrentProcessing(new CompareAndSetRunnerFactory(), i, "compareAndSet");
    }
  }

  private static void testConcurrentProcessing(RunnerFactory factory, int iteration, String test) throws Throwable {
      final TestTask[] tasks = new TestTask[TASK_COUNT];
      final AtomicInteger result = new AtomicInteger();

      for (int i = 0; i < TASK_COUNT; ++i) {
          tasks[i] = new TestTask(Integer.valueOf(i+1), result::addAndGet);
      }

      Thread[] threads = new Thread[THREAD_COUNT];
      for (int i = 0; i < THREAD_COUNT; ++i) {
          threads[i] = factory.createRunner(tasks);
      }

      for (int i = 0; i < THREAD_COUNT; ++i) {
          threads[i].start();
      }

      for (int i = 0; i < THREAD_COUNT; ++i) {
          threads[i].join();
      }

      check(result.get(), (TASK_COUNT >> 1) * (TASK_COUNT + 1),
              test + " test result not as expected", iteration);
  }

}
