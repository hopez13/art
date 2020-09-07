/*
 * Copyright (C) 2020 The Android Open Source Project
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

import java.io.PrintStream;
import java.lang.invoke.MethodHandles;
import java.lang.invoke.VarHandle;

class MicroVarHandleBenchmark extends BenchmarkBase {
  private static final int EXERCISE_ITERATIONS = 1000;

  MicroVarHandleBenchmark() {
    super(null);
  }

  @Override
  public String getName() {
    return getClass().getSimpleName();
  }

  @Override
  public void exercise() throws Throwable {
    for (int i = 0; i < EXERCISE_ITERATIONS; ++i) {
      run();
    }
  }

  @Override
  public void report() {
    try {
      double microseconds = measure() / EXERCISE_ITERATIONS;
      System.out.println(getName() + "(RunTimeRaw): " + microseconds + " us.");
    } catch (Throwable t) {
      System.err.println("Exception during the execution of " + getName());
      System.err.println(t);
      t.printStackTrace(new PrintStream(System.err));
      System.exit(1);
    }
  }
}

class VarHandleGetStaticFieldIntBenchmark extends MicroVarHandleBenchmark {
  static final int FIELD_VALUE = 42;
  static int field = FIELD_VALUE;

  VarHandle vh;

  VarHandleGetStaticFieldIntBenchmark() {
    try {
      vh = MethodHandles.lookup().findStaticVarHandle(VarHandleGetStaticFieldIntBenchmark.class,
                                                      "field", int.class);
    } catch (Throwable t) {
      vh = null;
    }
  }

  @Override
  public void setup() {
    int value = (int) vh.get();
    if (value != FIELD_VALUE) {
      throw new RuntimeException("vh.get() returned " + value);
    }
  }

  @Override
  public void run() {
    int x = (int) vh.get();
  }
}

class VarHandleGetStaticFieldStringBenchmark extends MicroVarHandleBenchmark {
  static final String FIELD_VALUE = "a string";
  static String field = FIELD_VALUE;

  VarHandle vh;

  VarHandleGetStaticFieldStringBenchmark() {
    try {
      vh = MethodHandles.lookup().findStaticVarHandle(VarHandleGetStaticFieldStringBenchmark.class,
                                                      "field", String.class);
    } catch (Throwable t) {
      vh = null;
    }
  }

  @Override
  public void setup() {
    String value = (String) vh.get();
    if (!value.equals(FIELD_VALUE)) {
      throw new RuntimeException("vh.get() returned " + value);
    }
  }

  @Override
  public void run() {
    String x = (String) vh.get();
  }
}

class VarHandleGetFieldIntBenchmark extends MicroVarHandleBenchmark {
  static final int FIELD_VALUE = 42;
  int field = FIELD_VALUE;
  VarHandle vh;

  VarHandleGetFieldIntBenchmark() {
    try {
      vh = MethodHandles.lookup().findVarHandle(VarHandleGetFieldIntBenchmark.class,
                                                "field", int.class);
    } catch (Throwable t) {
      vh = null;
    }
  }

  @Override
  public void setup() {
    int value = (int) vh.get(this);
    if (value != FIELD_VALUE) {
      throw new RuntimeException("vh.get() returned " + value);
    }
  }

  @Override
  public void run() {
    int x = (int) vh.get(this);
  }
}

class VarHandleGetFieldStringBenchmark extends MicroVarHandleBenchmark {
  static final String FIELD_VALUE = "field value";
  String field = FIELD_VALUE;
  VarHandle vh;

  VarHandleGetFieldStringBenchmark() {
    try {
      vh = MethodHandles.lookup().findVarHandle(VarHandleGetFieldStringBenchmark.class,
                                                "field", String.class);
    } catch (Throwable t) {
      vh = null;
    }
  }

  @Override
  public void setup() {
    String value = (String) vh.get(this);
    if (!value.equals(FIELD_VALUE)) {
      throw new RuntimeException("vh.get() returned " + value);
    }
  }

  @Override
  public void run() {
    String x = (String) vh.get(this);
  }
}

class VarHandleSetStaticFieldIntBenchmark extends MicroVarHandleBenchmark {
  static final int FIELD_VALUE = 42;
  static int field = ~FIELD_VALUE;

  VarHandle vh;

  VarHandleSetStaticFieldIntBenchmark() {
    try {
      vh = MethodHandles.lookup().findStaticVarHandle(VarHandleSetStaticFieldIntBenchmark.class,
                                                      "field", int.class);
    } catch (Throwable t) {
      vh = null;
    }
  }

  @Override
  public void teardown() {
    if (field != FIELD_VALUE) {
      throw new RuntimeException("field has unexpected value: " + field);
    }
  }

  @Override
  public void run() {
    vh.set(FIELD_VALUE);
  }
}

class VarHandleSetStaticFieldStringBenchmark extends MicroVarHandleBenchmark {
  static final String FIELD_VALUE = "a string";
  static String field = null;

  VarHandle vh;

  VarHandleSetStaticFieldStringBenchmark() {
    try {
      vh = MethodHandles.lookup().findStaticVarHandle(VarHandleSetStaticFieldStringBenchmark.class,
                                                      "field", String.class);
    } catch (Throwable t) {
      vh = null;
    }
  }

  @Override
  public void teardown() {
    if (!field.equals(FIELD_VALUE)) {
      throw new RuntimeException("field has unexpected value: " + field);
    }
  }

  @Override
  public void run() {
    vh.set(FIELD_VALUE);
  }
}

class VarHandleSetFieldIntBenchmark extends MicroVarHandleBenchmark {
  static final int FIELD_VALUE = 42;
  int field = ~FIELD_VALUE;
  VarHandle vh;

  VarHandleSetFieldIntBenchmark() {
    try {
      vh = MethodHandles.lookup().findVarHandle(VarHandleSetFieldIntBenchmark.class,
                                                "field", int.class);
    } catch (Throwable t) {
      vh = null;
    }
  }

  @Override
  public void teardown() {
    if (field != FIELD_VALUE) {
      throw new RuntimeException("field has unexpected value: " + field);
    }
  }

  @Override
  public void run() {
    vh.set(this, FIELD_VALUE);
  }
}

class VarHandleSetFieldStringBenchmark extends MicroVarHandleBenchmark {
  static final String FIELD_VALUE = "field value";
  String field = null;
  VarHandle vh;

  VarHandleSetFieldStringBenchmark() {
    try {
      vh = MethodHandles.lookup().findVarHandle(VarHandleSetFieldStringBenchmark.class,
                                                "field", String.class);
    } catch (Throwable t) {
      vh = null;
    }
  }

  @Override
  public void teardown() {
    if (!field.equals(FIELD_VALUE)) {
      throw new RuntimeException("field has unexpected value: " + field);
    }
  }

  @Override
  public void run() {
    vh.set(this, FIELD_VALUE);
  }
}

class MicroVarHandles {
  static final MicroVarHandleBenchmark [] benchmarks = {
    new VarHandleGetStaticFieldIntBenchmark(),
    new VarHandleGetStaticFieldStringBenchmark(),
    new VarHandleGetFieldIntBenchmark(),
    new VarHandleGetFieldStringBenchmark(),
    new VarHandleSetStaticFieldIntBenchmark(),
    new VarHandleSetStaticFieldStringBenchmark(),
    new VarHandleSetFieldIntBenchmark(),
    new VarHandleSetFieldStringBenchmark(),
  };

  private static void runAllBenchmarks() throws Throwable {
    for (MicroVarHandleBenchmark benchmark : benchmarks) {
      benchmark.report();
    }
  }

  static void main(String[] args) throws Throwable {
    runAllBenchmarks();
  }
}