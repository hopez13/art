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

import dalvik.system.VMDebug;

import java.lang.reflect.Method;
import java.util.ArrayList;
import java.util.List;
import java.util.Random;
import java.util.concurrent.atomic.AtomicInteger;
import java.util.concurrent.Callable;
import java.util.concurrent.ExecutionException;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Future;
import java.util.concurrent.LinkedBlockingQueue;
import java.util.concurrent.ThreadPoolExecutor;
import java.util.concurrent.TimeUnit;

/**
 * A test driver for JIT compiling methods and looking for JIT
 * cache issues.
 */
class Main {
  private static final int TASK_ITERATIONS = 10000;
  private static final String JITTED_METHOD = "$noinline$Call";
  private static final int OVERSUBSCRIBED_CORES = 1;
  private static final int CONCURRENCY = Runtime.getRuntime().availableProcessors() + OVERSUBSCRIBED_CORES;

  private static final int NUMBER_OF_TASK_TYPES = 8;
  private static final int TASK_BITMASK = (1 << NUMBER_OF_TASK_TYPES) - 1;

  private final ExecutorService executorService;
  private int runMask = 0;

  private Main() {
    this.executorService = new ThreadPoolExecutor(CONCURRENCY, CONCURRENCY, 5000,
        TimeUnit.MILLISECONDS,new LinkedBlockingQueue<>());
  }

  private void shutdown() {
    this.executorService.shutdown();
  }

  private void runTasks(Callable<Integer> task) {
    try {
      ArrayList<Callable<Integer>> tasks = new ArrayList<>(CONCURRENCY);
      for (int i = 0; i < CONCURRENCY; ++i) {
        tasks.add(i, task);
      }

      List<Future<Integer>> results = executorService.invokeAll(tasks);
      for (Future<?> result : results) {
        result.get();
      }
    } catch (InterruptedException | ExecutionException e) {
      System.err.println(e);
      System.exit(-1);
    }
  }

  private static abstract class BaseTask implements Callable<Integer> {
    public Integer call() throws Exception {
      int iterations = TASK_ITERATIONS / CONCURRENCY + 1;
      for (int i = 0; i < iterations; ++i) {
        $noinline$Call();
      }
      return $noinline$Call();
    }

    protected abstract Integer $noinline$Call();
  }

  private static class TaskOne extends BaseTask {
    @Override
    protected Integer $noinline$Call() {
      return null;
    }
  }

  private static class TaskTwo extends BaseTask {
    @Override
    protected Integer $noinline$Call() {
      return 0;
    }
  }

  private static class TaskThree extends BaseTask {
    @Override
    protected Integer $noinline$Call() {
      int sum = 0;
      for (int i = 0; i < 3; ++i) {
        sum = i * (i + 1);
      }
      return sum;
    }
  }

  private static class TaskFour extends BaseTask {
    @Override
    protected Integer $noinline$Call() {
      int sum = 0;
      for (int i = 0; i < 10; ++i) {
        int bits = i;
        bits = ((bits >>> 1) & 0x55555555) | ((bits << 1) & 0x55555555);
        bits = ((bits >>> 2) & 0x33333333) | ((bits << 2) & 0x33333333);
        bits = ((bits >>> 4) & 0x0f0f0f0f) | ((bits << 4) & 0x0f0f0f0f);
        bits = ((bits >>> 8) & 0x00ff00ff) | ((bits << 8) & 0x00ff00ff);
        bits = (bits >>> 16) | (bits << 16);
        sum += bits;
      }
      return sum;
    }
  }

  private static class TaskFive extends BaseTask {
    static final AtomicInteger instances = new AtomicInteger(0);
    int instance;
    TaskFive() {
      instance = instances.getAndIncrement();
    }
    protected Integer $noinline$Call() {
      return instance;
    }
  }

  private static class TaskSix extends TaskFive {
    protected Integer $noinline$Call() {
      return instance + 1;
    }
  }

  private static class TaskSeven extends TaskFive {
    protected Integer $noinline$Call() {
      return 2 * instance + 1;
    }
  }

  private static class TaskEight extends TaskFive {
    protected Integer $noinline$Call() {
      double a = Math.cosh(2.22 * instance);
      double b = a / 2;
      double c = b * 3;
      double d = a + b + c;
      if (d > 42) {
        d *= Math.max(Math.sin(d), Math.sinh(d));
        d *= Math.max(1.33, 0.17 * Math.sinh(d));
        d *= Math.max(1.34, 0.21 * Math.sinh(d));
        d *= Math.max(1.35, 0.32 * Math.sinh(d));
        d *= Math.max(1.36, 0.41 * Math.sinh(d));
        d *= Math.max(1.37, 0.57 * Math.sinh(d));
        d *= Math.max(1.38, 0.61 * Math.sinh(d));
        d *= Math.max(1.39, 0.79 * Math.sinh(d));
        d += Double.parseDouble("3.711e23");
      }

      if (d > 3) {
        return (int) a;
      } else {
        return (int) b;
      }
    }
  }

  private static boolean bitIsSet(final int mask, final int bit) {
    return ((mask >> bit) & 1) == 1;
  }

  private void runAndJitMethods(int mask) {
    if (bitIsSet(mask, 0)) {
      runTasks(new TaskOne());
    }
    if (bitIsSet(mask, 1)) {
      runTasks(new TaskTwo());
    }
    if (bitIsSet(mask, 2)) {
      runTasks(new TaskThree());
    }
    if (bitIsSet(mask, 3)) {
      runTasks(new TaskFour());
    }
    if (bitIsSet(mask, 4)) {
      runTasks(new TaskFive());
    }
    if (bitIsSet(mask, 5)) {
      runTasks(new TaskSix());
    }
    if (bitIsSet(mask, 6)) {
      runTasks(new TaskSeven());
    }
    if (bitIsSet(mask, 7)) {
      runTasks(new TaskEight());
    }
    runMask |= mask;
  }

  private Method lookupMethod(Class<?> klass, String name) {
    try {
      return klass.getDeclaredMethod(name);
    } catch (NoSuchMethodException e) {
      System.err.println(e);
      System.exit(-1);
      return null;
    }
  }

  private void removeJittedMethod(Class<?> klass, String name) {
    Method method = lookupMethod(klass, name);
    boolean removed = VMDebug.removeJitCompiledMethod(method);
    if (!removed) {
      throw new IllegalStateException("Method not in jit cache: " + klass + " " + name);
    }
  }

  private void removeJittedMethods(int mask) {
    int removeMask = runMask & mask;
    if (bitIsSet(removeMask, 0)) {
      removeJittedMethod(TaskOne.class, JITTED_METHOD);
    }
    if (bitIsSet(removeMask, 1)) {
      removeJittedMethod(TaskTwo.class, JITTED_METHOD);
    }
    if (bitIsSet(removeMask, 2)) {
      removeJittedMethod(TaskThree.class, JITTED_METHOD);
    }
    if (bitIsSet(removeMask, 3)) {
      removeJittedMethod(TaskFour.class, JITTED_METHOD);
    }
    if (bitIsSet(removeMask, 4)) {
      removeJittedMethod(TaskFive.class, JITTED_METHOD);
    }
    if (bitIsSet(removeMask, 5)) {
      removeJittedMethod(TaskSix.class, JITTED_METHOD);
    }
    if (bitIsSet(removeMask, 6)) {
      removeJittedMethod(TaskSeven.class, JITTED_METHOD);
    }
    if (bitIsSet(removeMask, 7)) {
      removeJittedMethod(TaskEight.class, JITTED_METHOD);
    }
    runMask ^= removeMask;
  }

  private static void runTest() {
    Main concurrentExecution = new Main();
    Random methodsGenerator = new Random(5);
    Random flushGenerator = new Random(7);
    try {
      for (int i = 0; i < 32; ++i) {
        concurrentExecution.runAndJitMethods(methodsGenerator.nextInt(TASK_BITMASK) + 1);
        concurrentExecution.removeJittedMethods(flushGenerator.nextInt(TASK_BITMASK) + 1);
      }
    } finally {
      concurrentExecution.shutdown();
    }
  }

  public static void main(String[] args) {
    if (VMDebug.isJitCompilationEnabled()) {
      runTest();
    }
    System.out.println("Done");
  }
}
