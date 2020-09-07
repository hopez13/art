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

import java.lang.invoke.MethodHandles;
import java.lang.invoke.VarHandle;
import java.lang.reflect.Field;
import java.nio.ByteOrder;
import sun.misc.Unsafe;

class VarHandleGetStaticFieldIntBenchmark extends MicroBenchmark {
  static final int FIELD_VALUE = 42;

  static int field = FIELD_VALUE;
  VarHandle vh;

  VarHandleGetStaticFieldIntBenchmark() throws Throwable {
    vh = MethodHandles.lookup().findStaticVarHandle(this.getClass(), "field", int.class);
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
    int x;
    for (int pass = 0; pass < 100; ++pass) {
      x = (int) vh.get(); // OP
      x = (int) vh.get(); // OP
      x = (int) vh.get(); // OP
      x = (int) vh.get(); // OP
      x = (int) vh.get(); // OP
      x = (int) vh.get(); // OP
      x = (int) vh.get(); // OP
      x = (int) vh.get(); // OP
      x = (int) vh.get(); // OP
      x = (int) vh.get(); // OP
      x = (int) vh.get(); // OP
      x = (int) vh.get(); // OP
      x = (int) vh.get(); // OP
      x = (int) vh.get(); // OP
      x = (int) vh.get(); // OP
      x = (int) vh.get(); // OP
      x = (int) vh.get(); // OP
      x = (int) vh.get(); // OP
      x = (int) vh.get(); // OP
      x = (int) vh.get(); // OP
      x = (int) vh.get(); // OP
      x = (int) vh.get(); // OP
      x = (int) vh.get(); // OP
      x = (int) vh.get(); // OP
      x = (int) vh.get(); // OP
      x = (int) vh.get(); // OP
      x = (int) vh.get(); // OP
      x = (int) vh.get(); // OP
      x = (int) vh.get(); // OP
      x = (int) vh.get(); // OP
    }
  }

  @Override
  public int innerIterations() {
      return 3000;
  }
}

class VarHandleGetStaticFieldStringBenchmark extends MicroBenchmark {
  static final String FIELD_VALUE = "a string";

  static String field = FIELD_VALUE;
  VarHandle vh;

  VarHandleGetStaticFieldStringBenchmark() throws Throwable {
    vh = MethodHandles.lookup().findStaticVarHandle(this.getClass(), "field", String.class);
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
    String x;
    for (int pass = 0; pass < 100; ++pass) {
      x = (String) vh.get(); // OP
      x = (String) vh.get(); // OP
      x = (String) vh.get(); // OP
      x = (String) vh.get(); // OP
      x = (String) vh.get(); // OP
      x = (String) vh.get(); // OP
      x = (String) vh.get(); // OP
      x = (String) vh.get(); // OP
      x = (String) vh.get(); // OP
      x = (String) vh.get(); // OP
      x = (String) vh.get(); // OP
      x = (String) vh.get(); // OP
      x = (String) vh.get(); // OP
      x = (String) vh.get(); // OP
      x = (String) vh.get(); // OP
      x = (String) vh.get(); // OP
      x = (String) vh.get(); // OP
      x = (String) vh.get(); // OP
      x = (String) vh.get(); // OP
      x = (String) vh.get(); // OP
      x = (String) vh.get(); // OP
      x = (String) vh.get(); // OP
      x = (String) vh.get(); // OP
      x = (String) vh.get(); // OP
      x = (String) vh.get(); // OP
      x = (String) vh.get(); // OP
      x = (String) vh.get(); // OP
      x = (String) vh.get(); // OP
      x = (String) vh.get(); // OP
      x = (String) vh.get(); // OP
    }
  }

  @Override
  public int innerIterations() {
      return 3000;
  }
}

class VarHandleGetFieldIntBenchmark extends MicroBenchmark {
  static final int FIELD_VALUE = 42;

  int field = FIELD_VALUE;
  VarHandle vh;

  VarHandleGetFieldIntBenchmark() throws Throwable {
    vh = MethodHandles.lookup().findVarHandle(this.getClass(), "field", int.class);
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
    int x;
    for (int pass = 0; pass < 100; ++pass) {
      x = (int) vh.get(this); // OP
      x = (int) vh.get(this); // OP
      x = (int) vh.get(this); // OP
      x = (int) vh.get(this); // OP
      x = (int) vh.get(this); // OP
      x = (int) vh.get(this); // OP
      x = (int) vh.get(this); // OP
      x = (int) vh.get(this); // OP
      x = (int) vh.get(this); // OP
      x = (int) vh.get(this); // OP
      x = (int) vh.get(this); // OP
      x = (int) vh.get(this); // OP
      x = (int) vh.get(this); // OP
      x = (int) vh.get(this); // OP
      x = (int) vh.get(this); // OP
      x = (int) vh.get(this); // OP
      x = (int) vh.get(this); // OP
      x = (int) vh.get(this); // OP
      x = (int) vh.get(this); // OP
      x = (int) vh.get(this); // OP
      x = (int) vh.get(this); // OP
      x = (int) vh.get(this); // OP
      x = (int) vh.get(this); // OP
      x = (int) vh.get(this); // OP
      x = (int) vh.get(this); // OP
      x = (int) vh.get(this); // OP
      x = (int) vh.get(this); // OP
      x = (int) vh.get(this); // OP
      x = (int) vh.get(this); // OP
      x = (int) vh.get(this); // OP
    }
  }

  @Override
  public int innerIterations() {
      return 3000;
  }
}

class VarHandleGetFieldStringBenchmark extends MicroBenchmark {
  static final String FIELD_VALUE = "field value";

  String field = FIELD_VALUE;
  VarHandle vh;

  VarHandleGetFieldStringBenchmark() throws Throwable {
    vh = MethodHandles.lookup().findVarHandle(this.getClass(), "field", String.class);
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
    String x;
    for (int pass = 0; pass < 100; ++pass) {
      x = (String) vh.get(this); // OP
      x = (String) vh.get(this); // OP
      x = (String) vh.get(this); // OP
      x = (String) vh.get(this); // OP
      x = (String) vh.get(this); // OP
      x = (String) vh.get(this); // OP
      x = (String) vh.get(this); // OP
      x = (String) vh.get(this); // OP
      x = (String) vh.get(this); // OP
      x = (String) vh.get(this); // OP
      x = (String) vh.get(this); // OP
      x = (String) vh.get(this); // OP
      x = (String) vh.get(this); // OP
      x = (String) vh.get(this); // OP
      x = (String) vh.get(this); // OP
      x = (String) vh.get(this); // OP
      x = (String) vh.get(this); // OP
      x = (String) vh.get(this); // OP
      x = (String) vh.get(this); // OP
      x = (String) vh.get(this); // OP
      x = (String) vh.get(this); // OP
      x = (String) vh.get(this); // OP
      x = (String) vh.get(this); // OP
      x = (String) vh.get(this); // OP
      x = (String) vh.get(this); // OP
      x = (String) vh.get(this); // OP
      x = (String) vh.get(this); // OP
      x = (String) vh.get(this); // OP
      x = (String) vh.get(this); // OP
      x = (String) vh.get(this); // OP
    }
  }

  @Override
  public int innerIterations() {
      return 3000;
  }
}

class VarHandleGetAcquireStaticFieldIntBenchmark extends MicroBenchmark {
  static final int FIELD_VALUE = 42;

  static int field = FIELD_VALUE;
  VarHandle vh;

  VarHandleGetAcquireStaticFieldIntBenchmark() throws Throwable {
    vh = MethodHandles.lookup().findStaticVarHandle(this.getClass(), "field", int.class);
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
    int x;
    for (int pass = 0; pass < 100; ++pass) {
      x = (int) vh.getAcquire(); // OP
      x = (int) vh.getAcquire(); // OP
      x = (int) vh.getAcquire(); // OP
      x = (int) vh.getAcquire(); // OP
      x = (int) vh.getAcquire(); // OP
      x = (int) vh.getAcquire(); // OP
      x = (int) vh.getAcquire(); // OP
      x = (int) vh.getAcquire(); // OP
      x = (int) vh.getAcquire(); // OP
      x = (int) vh.getAcquire(); // OP
      x = (int) vh.getAcquire(); // OP
      x = (int) vh.getAcquire(); // OP
      x = (int) vh.getAcquire(); // OP
      x = (int) vh.getAcquire(); // OP
      x = (int) vh.getAcquire(); // OP
      x = (int) vh.getAcquire(); // OP
      x = (int) vh.getAcquire(); // OP
      x = (int) vh.getAcquire(); // OP
      x = (int) vh.getAcquire(); // OP
      x = (int) vh.getAcquire(); // OP
      x = (int) vh.getAcquire(); // OP
      x = (int) vh.getAcquire(); // OP
      x = (int) vh.getAcquire(); // OP
      x = (int) vh.getAcquire(); // OP
      x = (int) vh.getAcquire(); // OP
      x = (int) vh.getAcquire(); // OP
      x = (int) vh.getAcquire(); // OP
      x = (int) vh.getAcquire(); // OP
      x = (int) vh.getAcquire(); // OP
      x = (int) vh.getAcquire(); // OP
    }
  }

  @Override
  public int innerIterations() {
      return 3000;
  }
}

class VarHandleGetAcquireStaticFieldStringBenchmark extends MicroBenchmark {
  static final String FIELD_VALUE = "a string";

  static String field = FIELD_VALUE;
  VarHandle vh;

  VarHandleGetAcquireStaticFieldStringBenchmark() throws Throwable {
    vh = MethodHandles.lookup().findStaticVarHandle(this.getClass(), "field", String.class);
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
    String x;
    for (int pass = 0; pass < 100; ++pass) {
      x = (String) vh.getAcquire(); // OP
      x = (String) vh.getAcquire(); // OP
      x = (String) vh.getAcquire(); // OP
      x = (String) vh.getAcquire(); // OP
      x = (String) vh.getAcquire(); // OP
      x = (String) vh.getAcquire(); // OP
      x = (String) vh.getAcquire(); // OP
      x = (String) vh.getAcquire(); // OP
      x = (String) vh.getAcquire(); // OP
      x = (String) vh.getAcquire(); // OP
      x = (String) vh.getAcquire(); // OP
      x = (String) vh.getAcquire(); // OP
      x = (String) vh.getAcquire(); // OP
      x = (String) vh.getAcquire(); // OP
      x = (String) vh.getAcquire(); // OP
      x = (String) vh.getAcquire(); // OP
      x = (String) vh.getAcquire(); // OP
      x = (String) vh.getAcquire(); // OP
      x = (String) vh.getAcquire(); // OP
      x = (String) vh.getAcquire(); // OP
      x = (String) vh.getAcquire(); // OP
      x = (String) vh.getAcquire(); // OP
      x = (String) vh.getAcquire(); // OP
      x = (String) vh.getAcquire(); // OP
      x = (String) vh.getAcquire(); // OP
      x = (String) vh.getAcquire(); // OP
      x = (String) vh.getAcquire(); // OP
      x = (String) vh.getAcquire(); // OP
      x = (String) vh.getAcquire(); // OP
      x = (String) vh.getAcquire(); // OP
    }
  }

  @Override
  public int innerIterations() {
      return 3000;
  }
}

class VarHandleGetAcquireFieldIntBenchmark extends MicroBenchmark {
  static final int FIELD_VALUE = 42;

  int field = FIELD_VALUE;
  VarHandle vh;

  VarHandleGetAcquireFieldIntBenchmark() throws Throwable {
    vh = MethodHandles.lookup().findVarHandle(this.getClass(), "field", int.class);
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
    int x;
    for (int pass = 0; pass < 100; ++pass) {
      x = (int) vh.getAcquire(this); // OP
      x = (int) vh.getAcquire(this); // OP
      x = (int) vh.getAcquire(this); // OP
      x = (int) vh.getAcquire(this); // OP
      x = (int) vh.getAcquire(this); // OP
      x = (int) vh.getAcquire(this); // OP
      x = (int) vh.getAcquire(this); // OP
      x = (int) vh.getAcquire(this); // OP
      x = (int) vh.getAcquire(this); // OP
      x = (int) vh.getAcquire(this); // OP
      x = (int) vh.getAcquire(this); // OP
      x = (int) vh.getAcquire(this); // OP
      x = (int) vh.getAcquire(this); // OP
      x = (int) vh.getAcquire(this); // OP
      x = (int) vh.getAcquire(this); // OP
      x = (int) vh.getAcquire(this); // OP
      x = (int) vh.getAcquire(this); // OP
      x = (int) vh.getAcquire(this); // OP
      x = (int) vh.getAcquire(this); // OP
      x = (int) vh.getAcquire(this); // OP
      x = (int) vh.getAcquire(this); // OP
      x = (int) vh.getAcquire(this); // OP
      x = (int) vh.getAcquire(this); // OP
      x = (int) vh.getAcquire(this); // OP
      x = (int) vh.getAcquire(this); // OP
      x = (int) vh.getAcquire(this); // OP
      x = (int) vh.getAcquire(this); // OP
      x = (int) vh.getAcquire(this); // OP
      x = (int) vh.getAcquire(this); // OP
      x = (int) vh.getAcquire(this); // OP
    }
  }

  @Override
  public int innerIterations() {
      return 3000;
  }
}

class VarHandleGetAcquireFieldStringBenchmark extends MicroBenchmark {
  static final String FIELD_VALUE = "field value";

  String field = FIELD_VALUE;
  VarHandle vh;

  VarHandleGetAcquireFieldStringBenchmark() throws Throwable {
    vh = MethodHandles.lookup().findVarHandle(this.getClass(), "field", String.class);
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
    String x;
    for (int pass = 0; pass < 100; ++pass) {
      x = (String) vh.getAcquire(this); // OP
      x = (String) vh.getAcquire(this); // OP
      x = (String) vh.getAcquire(this); // OP
      x = (String) vh.getAcquire(this); // OP
      x = (String) vh.getAcquire(this); // OP
      x = (String) vh.getAcquire(this); // OP
      x = (String) vh.getAcquire(this); // OP
      x = (String) vh.getAcquire(this); // OP
      x = (String) vh.getAcquire(this); // OP
      x = (String) vh.getAcquire(this); // OP
      x = (String) vh.getAcquire(this); // OP
      x = (String) vh.getAcquire(this); // OP
      x = (String) vh.getAcquire(this); // OP
      x = (String) vh.getAcquire(this); // OP
      x = (String) vh.getAcquire(this); // OP
      x = (String) vh.getAcquire(this); // OP
      x = (String) vh.getAcquire(this); // OP
      x = (String) vh.getAcquire(this); // OP
      x = (String) vh.getAcquire(this); // OP
      x = (String) vh.getAcquire(this); // OP
      x = (String) vh.getAcquire(this); // OP
      x = (String) vh.getAcquire(this); // OP
      x = (String) vh.getAcquire(this); // OP
      x = (String) vh.getAcquire(this); // OP
      x = (String) vh.getAcquire(this); // OP
      x = (String) vh.getAcquire(this); // OP
      x = (String) vh.getAcquire(this); // OP
      x = (String) vh.getAcquire(this); // OP
      x = (String) vh.getAcquire(this); // OP
      x = (String) vh.getAcquire(this); // OP
    }
  }

  @Override
  public int innerIterations() {
      return 3000;
  }
}

class VarHandleGetOpaqueStaticFieldIntBenchmark extends MicroBenchmark {
  static final int FIELD_VALUE = 42;

  static int field = FIELD_VALUE;
  VarHandle vh;

  VarHandleGetOpaqueStaticFieldIntBenchmark() throws Throwable {
    vh = MethodHandles.lookup().findStaticVarHandle(this.getClass(), "field", int.class);
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
    int x;
    for (int pass = 0; pass < 100; ++pass) {
      x = (int) vh.getOpaque(); // OP
      x = (int) vh.getOpaque(); // OP
      x = (int) vh.getOpaque(); // OP
      x = (int) vh.getOpaque(); // OP
      x = (int) vh.getOpaque(); // OP
      x = (int) vh.getOpaque(); // OP
      x = (int) vh.getOpaque(); // OP
      x = (int) vh.getOpaque(); // OP
      x = (int) vh.getOpaque(); // OP
      x = (int) vh.getOpaque(); // OP
      x = (int) vh.getOpaque(); // OP
      x = (int) vh.getOpaque(); // OP
      x = (int) vh.getOpaque(); // OP
      x = (int) vh.getOpaque(); // OP
      x = (int) vh.getOpaque(); // OP
      x = (int) vh.getOpaque(); // OP
      x = (int) vh.getOpaque(); // OP
      x = (int) vh.getOpaque(); // OP
      x = (int) vh.getOpaque(); // OP
      x = (int) vh.getOpaque(); // OP
      x = (int) vh.getOpaque(); // OP
      x = (int) vh.getOpaque(); // OP
      x = (int) vh.getOpaque(); // OP
      x = (int) vh.getOpaque(); // OP
      x = (int) vh.getOpaque(); // OP
      x = (int) vh.getOpaque(); // OP
      x = (int) vh.getOpaque(); // OP
      x = (int) vh.getOpaque(); // OP
      x = (int) vh.getOpaque(); // OP
      x = (int) vh.getOpaque(); // OP
    }
  }

  @Override
  public int innerIterations() {
      return 3000;
  }
}

class VarHandleGetOpaqueStaticFieldStringBenchmark extends MicroBenchmark {
  static final String FIELD_VALUE = "a string";

  static String field = FIELD_VALUE;
  VarHandle vh;

  VarHandleGetOpaqueStaticFieldStringBenchmark() throws Throwable {
    vh = MethodHandles.lookup().findStaticVarHandle(this.getClass(), "field", String.class);
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
    String x;
    for (int pass = 0; pass < 100; ++pass) {
      x = (String) vh.getOpaque(); // OP
      x = (String) vh.getOpaque(); // OP
      x = (String) vh.getOpaque(); // OP
      x = (String) vh.getOpaque(); // OP
      x = (String) vh.getOpaque(); // OP
      x = (String) vh.getOpaque(); // OP
      x = (String) vh.getOpaque(); // OP
      x = (String) vh.getOpaque(); // OP
      x = (String) vh.getOpaque(); // OP
      x = (String) vh.getOpaque(); // OP
      x = (String) vh.getOpaque(); // OP
      x = (String) vh.getOpaque(); // OP
      x = (String) vh.getOpaque(); // OP
      x = (String) vh.getOpaque(); // OP
      x = (String) vh.getOpaque(); // OP
      x = (String) vh.getOpaque(); // OP
      x = (String) vh.getOpaque(); // OP
      x = (String) vh.getOpaque(); // OP
      x = (String) vh.getOpaque(); // OP
      x = (String) vh.getOpaque(); // OP
      x = (String) vh.getOpaque(); // OP
      x = (String) vh.getOpaque(); // OP
      x = (String) vh.getOpaque(); // OP
      x = (String) vh.getOpaque(); // OP
      x = (String) vh.getOpaque(); // OP
      x = (String) vh.getOpaque(); // OP
      x = (String) vh.getOpaque(); // OP
      x = (String) vh.getOpaque(); // OP
      x = (String) vh.getOpaque(); // OP
      x = (String) vh.getOpaque(); // OP
    }
  }

  @Override
  public int innerIterations() {
      return 3000;
  }
}

class VarHandleGetOpaqueFieldIntBenchmark extends MicroBenchmark {
  static final int FIELD_VALUE = 42;

  int field = FIELD_VALUE;
  VarHandle vh;

  VarHandleGetOpaqueFieldIntBenchmark() throws Throwable {
    vh = MethodHandles.lookup().findVarHandle(this.getClass(), "field", int.class);
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
    int x;
    for (int pass = 0; pass < 100; ++pass) {
      x = (int) vh.getOpaque(this); // OP
      x = (int) vh.getOpaque(this); // OP
      x = (int) vh.getOpaque(this); // OP
      x = (int) vh.getOpaque(this); // OP
      x = (int) vh.getOpaque(this); // OP
      x = (int) vh.getOpaque(this); // OP
      x = (int) vh.getOpaque(this); // OP
      x = (int) vh.getOpaque(this); // OP
      x = (int) vh.getOpaque(this); // OP
      x = (int) vh.getOpaque(this); // OP
      x = (int) vh.getOpaque(this); // OP
      x = (int) vh.getOpaque(this); // OP
      x = (int) vh.getOpaque(this); // OP
      x = (int) vh.getOpaque(this); // OP
      x = (int) vh.getOpaque(this); // OP
      x = (int) vh.getOpaque(this); // OP
      x = (int) vh.getOpaque(this); // OP
      x = (int) vh.getOpaque(this); // OP
      x = (int) vh.getOpaque(this); // OP
      x = (int) vh.getOpaque(this); // OP
      x = (int) vh.getOpaque(this); // OP
      x = (int) vh.getOpaque(this); // OP
      x = (int) vh.getOpaque(this); // OP
      x = (int) vh.getOpaque(this); // OP
      x = (int) vh.getOpaque(this); // OP
      x = (int) vh.getOpaque(this); // OP
      x = (int) vh.getOpaque(this); // OP
      x = (int) vh.getOpaque(this); // OP
      x = (int) vh.getOpaque(this); // OP
      x = (int) vh.getOpaque(this); // OP
    }
  }

  @Override
  public int innerIterations() {
      return 3000;
  }
}

class VarHandleGetOpaqueFieldStringBenchmark extends MicroBenchmark {
  static final String FIELD_VALUE = "field value";

  String field = FIELD_VALUE;
  VarHandle vh;

  VarHandleGetOpaqueFieldStringBenchmark() throws Throwable {
    vh = MethodHandles.lookup().findVarHandle(this.getClass(), "field", String.class);
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
    String x;
    for (int pass = 0; pass < 100; ++pass) {
      x = (String) vh.getOpaque(this); // OP
      x = (String) vh.getOpaque(this); // OP
      x = (String) vh.getOpaque(this); // OP
      x = (String) vh.getOpaque(this); // OP
      x = (String) vh.getOpaque(this); // OP
      x = (String) vh.getOpaque(this); // OP
      x = (String) vh.getOpaque(this); // OP
      x = (String) vh.getOpaque(this); // OP
      x = (String) vh.getOpaque(this); // OP
      x = (String) vh.getOpaque(this); // OP
      x = (String) vh.getOpaque(this); // OP
      x = (String) vh.getOpaque(this); // OP
      x = (String) vh.getOpaque(this); // OP
      x = (String) vh.getOpaque(this); // OP
      x = (String) vh.getOpaque(this); // OP
      x = (String) vh.getOpaque(this); // OP
      x = (String) vh.getOpaque(this); // OP
      x = (String) vh.getOpaque(this); // OP
      x = (String) vh.getOpaque(this); // OP
      x = (String) vh.getOpaque(this); // OP
      x = (String) vh.getOpaque(this); // OP
      x = (String) vh.getOpaque(this); // OP
      x = (String) vh.getOpaque(this); // OP
      x = (String) vh.getOpaque(this); // OP
      x = (String) vh.getOpaque(this); // OP
      x = (String) vh.getOpaque(this); // OP
      x = (String) vh.getOpaque(this); // OP
      x = (String) vh.getOpaque(this); // OP
      x = (String) vh.getOpaque(this); // OP
      x = (String) vh.getOpaque(this); // OP
    }
  }

  @Override
  public int innerIterations() {
      return 3000;
  }
}

class VarHandleGetVolatileStaticFieldIntBenchmark extends MicroBenchmark {
  static final int FIELD_VALUE = 42;

  static int field = FIELD_VALUE;
  VarHandle vh;

  VarHandleGetVolatileStaticFieldIntBenchmark() throws Throwable {
    vh = MethodHandles.lookup().findStaticVarHandle(this.getClass(), "field", int.class);
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
    int x;
    for (int pass = 0; pass < 100; ++pass) {
      x = (int) vh.getVolatile(); // OP
      x = (int) vh.getVolatile(); // OP
      x = (int) vh.getVolatile(); // OP
      x = (int) vh.getVolatile(); // OP
      x = (int) vh.getVolatile(); // OP
      x = (int) vh.getVolatile(); // OP
      x = (int) vh.getVolatile(); // OP
      x = (int) vh.getVolatile(); // OP
      x = (int) vh.getVolatile(); // OP
      x = (int) vh.getVolatile(); // OP
      x = (int) vh.getVolatile(); // OP
      x = (int) vh.getVolatile(); // OP
      x = (int) vh.getVolatile(); // OP
      x = (int) vh.getVolatile(); // OP
      x = (int) vh.getVolatile(); // OP
      x = (int) vh.getVolatile(); // OP
      x = (int) vh.getVolatile(); // OP
      x = (int) vh.getVolatile(); // OP
      x = (int) vh.getVolatile(); // OP
      x = (int) vh.getVolatile(); // OP
      x = (int) vh.getVolatile(); // OP
      x = (int) vh.getVolatile(); // OP
      x = (int) vh.getVolatile(); // OP
      x = (int) vh.getVolatile(); // OP
      x = (int) vh.getVolatile(); // OP
      x = (int) vh.getVolatile(); // OP
      x = (int) vh.getVolatile(); // OP
      x = (int) vh.getVolatile(); // OP
      x = (int) vh.getVolatile(); // OP
      x = (int) vh.getVolatile(); // OP
    }
  }

  @Override
  public int innerIterations() {
      return 3000;
  }
}

class VarHandleGetVolatileStaticFieldStringBenchmark extends MicroBenchmark {
  static final String FIELD_VALUE = "a string";

  static String field = FIELD_VALUE;
  VarHandle vh;

  VarHandleGetVolatileStaticFieldStringBenchmark() throws Throwable {
    vh = MethodHandles.lookup().findStaticVarHandle(this.getClass(), "field", String.class);
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
    String x;
    for (int pass = 0; pass < 100; ++pass) {
      x = (String) vh.getVolatile(); // OP
      x = (String) vh.getVolatile(); // OP
      x = (String) vh.getVolatile(); // OP
      x = (String) vh.getVolatile(); // OP
      x = (String) vh.getVolatile(); // OP
      x = (String) vh.getVolatile(); // OP
      x = (String) vh.getVolatile(); // OP
      x = (String) vh.getVolatile(); // OP
      x = (String) vh.getVolatile(); // OP
      x = (String) vh.getVolatile(); // OP
      x = (String) vh.getVolatile(); // OP
      x = (String) vh.getVolatile(); // OP
      x = (String) vh.getVolatile(); // OP
      x = (String) vh.getVolatile(); // OP
      x = (String) vh.getVolatile(); // OP
      x = (String) vh.getVolatile(); // OP
      x = (String) vh.getVolatile(); // OP
      x = (String) vh.getVolatile(); // OP
      x = (String) vh.getVolatile(); // OP
      x = (String) vh.getVolatile(); // OP
      x = (String) vh.getVolatile(); // OP
      x = (String) vh.getVolatile(); // OP
      x = (String) vh.getVolatile(); // OP
      x = (String) vh.getVolatile(); // OP
      x = (String) vh.getVolatile(); // OP
      x = (String) vh.getVolatile(); // OP
      x = (String) vh.getVolatile(); // OP
      x = (String) vh.getVolatile(); // OP
      x = (String) vh.getVolatile(); // OP
      x = (String) vh.getVolatile(); // OP
    }
  }

  @Override
  public int innerIterations() {
      return 3000;
  }
}

class VarHandleGetVolatileFieldIntBenchmark extends MicroBenchmark {
  static final int FIELD_VALUE = 42;

  int field = FIELD_VALUE;
  VarHandle vh;

  VarHandleGetVolatileFieldIntBenchmark() throws Throwable {
    vh = MethodHandles.lookup().findVarHandle(this.getClass(), "field", int.class);
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
    int x;
    for (int pass = 0; pass < 100; ++pass) {
      x = (int) vh.getVolatile(this); // OP
      x = (int) vh.getVolatile(this); // OP
      x = (int) vh.getVolatile(this); // OP
      x = (int) vh.getVolatile(this); // OP
      x = (int) vh.getVolatile(this); // OP
      x = (int) vh.getVolatile(this); // OP
      x = (int) vh.getVolatile(this); // OP
      x = (int) vh.getVolatile(this); // OP
      x = (int) vh.getVolatile(this); // OP
      x = (int) vh.getVolatile(this); // OP
      x = (int) vh.getVolatile(this); // OP
      x = (int) vh.getVolatile(this); // OP
      x = (int) vh.getVolatile(this); // OP
      x = (int) vh.getVolatile(this); // OP
      x = (int) vh.getVolatile(this); // OP
      x = (int) vh.getVolatile(this); // OP
      x = (int) vh.getVolatile(this); // OP
      x = (int) vh.getVolatile(this); // OP
      x = (int) vh.getVolatile(this); // OP
      x = (int) vh.getVolatile(this); // OP
      x = (int) vh.getVolatile(this); // OP
      x = (int) vh.getVolatile(this); // OP
      x = (int) vh.getVolatile(this); // OP
      x = (int) vh.getVolatile(this); // OP
      x = (int) vh.getVolatile(this); // OP
      x = (int) vh.getVolatile(this); // OP
      x = (int) vh.getVolatile(this); // OP
      x = (int) vh.getVolatile(this); // OP
      x = (int) vh.getVolatile(this); // OP
      x = (int) vh.getVolatile(this); // OP
    }
  }

  @Override
  public int innerIterations() {
      return 3000;
  }
}

class VarHandleGetVolatileFieldStringBenchmark extends MicroBenchmark {
  static final String FIELD_VALUE = "field value";

  String field = FIELD_VALUE;
  VarHandle vh;

  VarHandleGetVolatileFieldStringBenchmark() throws Throwable {
    vh = MethodHandles.lookup().findVarHandle(this.getClass(), "field", String.class);
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
    String x;
    for (int pass = 0; pass < 100; ++pass) {
      x = (String) vh.getVolatile(this); // OP
      x = (String) vh.getVolatile(this); // OP
      x = (String) vh.getVolatile(this); // OP
      x = (String) vh.getVolatile(this); // OP
      x = (String) vh.getVolatile(this); // OP
      x = (String) vh.getVolatile(this); // OP
      x = (String) vh.getVolatile(this); // OP
      x = (String) vh.getVolatile(this); // OP
      x = (String) vh.getVolatile(this); // OP
      x = (String) vh.getVolatile(this); // OP
      x = (String) vh.getVolatile(this); // OP
      x = (String) vh.getVolatile(this); // OP
      x = (String) vh.getVolatile(this); // OP
      x = (String) vh.getVolatile(this); // OP
      x = (String) vh.getVolatile(this); // OP
      x = (String) vh.getVolatile(this); // OP
      x = (String) vh.getVolatile(this); // OP
      x = (String) vh.getVolatile(this); // OP
      x = (String) vh.getVolatile(this); // OP
      x = (String) vh.getVolatile(this); // OP
      x = (String) vh.getVolatile(this); // OP
      x = (String) vh.getVolatile(this); // OP
      x = (String) vh.getVolatile(this); // OP
      x = (String) vh.getVolatile(this); // OP
      x = (String) vh.getVolatile(this); // OP
      x = (String) vh.getVolatile(this); // OP
      x = (String) vh.getVolatile(this); // OP
      x = (String) vh.getVolatile(this); // OP
      x = (String) vh.getVolatile(this); // OP
      x = (String) vh.getVolatile(this); // OP
    }
  }

  @Override
  public int innerIterations() {
      return 3000;
  }
}

class VarHandleSetStaticFieldIntBenchmark extends MicroBenchmark {
  static final int FIELD_VALUE = 42;
  static int field = ~FIELD_VALUE;

  VarHandle vh;

  VarHandleSetStaticFieldIntBenchmark() throws Throwable {
    vh = MethodHandles.lookup().findStaticVarHandle(this.getClass(), "field", int.class);
  }

  @Override
  public void teardown() {
    if (field != FIELD_VALUE) {
      throw new RuntimeException("field has unexpected value: " + field);
    }
  }

  @Override
  public void run() {
    for (int pass = 0; pass < 100; ++pass) {
      vh.set(FIELD_VALUE); // OP
      vh.set(FIELD_VALUE); // OP
      vh.set(FIELD_VALUE); // OP
      vh.set(FIELD_VALUE); // OP
      vh.set(FIELD_VALUE); // OP
      vh.set(FIELD_VALUE); // OP
      vh.set(FIELD_VALUE); // OP
      vh.set(FIELD_VALUE); // OP
      vh.set(FIELD_VALUE); // OP
      vh.set(FIELD_VALUE); // OP
      vh.set(FIELD_VALUE); // OP
      vh.set(FIELD_VALUE); // OP
      vh.set(FIELD_VALUE); // OP
      vh.set(FIELD_VALUE); // OP
      vh.set(FIELD_VALUE); // OP
      vh.set(FIELD_VALUE); // OP
      vh.set(FIELD_VALUE); // OP
      vh.set(FIELD_VALUE); // OP
      vh.set(FIELD_VALUE); // OP
      vh.set(FIELD_VALUE); // OP
      vh.set(FIELD_VALUE); // OP
      vh.set(FIELD_VALUE); // OP
      vh.set(FIELD_VALUE); // OP
      vh.set(FIELD_VALUE); // OP
      vh.set(FIELD_VALUE); // OP
      vh.set(FIELD_VALUE); // OP
      vh.set(FIELD_VALUE); // OP
      vh.set(FIELD_VALUE); // OP
      vh.set(FIELD_VALUE); // OP
      vh.set(FIELD_VALUE); // OP
    }
  }

  @Override
  public int innerIterations() {
      return 3000;
  }
}

class VarHandleSetStaticFieldStringBenchmark extends MicroBenchmark {
  static final String FIELD_VALUE = "a string";
  static String field = null;

  VarHandle vh;

  VarHandleSetStaticFieldStringBenchmark() throws Throwable {
    vh = MethodHandles.lookup().findStaticVarHandle(this.getClass(), "field", String.class);
  }

  @Override
  public void teardown() {
    if (!field.equals(FIELD_VALUE)) {
      throw new RuntimeException("field has unexpected value: " + field);
    }
  }

  @Override
  public void run() {
    for (int pass = 0; pass < 100; ++pass) {
      vh.set(FIELD_VALUE); // OP
      vh.set(FIELD_VALUE); // OP
      vh.set(FIELD_VALUE); // OP
      vh.set(FIELD_VALUE); // OP
      vh.set(FIELD_VALUE); // OP
      vh.set(FIELD_VALUE); // OP
      vh.set(FIELD_VALUE); // OP
      vh.set(FIELD_VALUE); // OP
      vh.set(FIELD_VALUE); // OP
      vh.set(FIELD_VALUE); // OP
      vh.set(FIELD_VALUE); // OP
      vh.set(FIELD_VALUE); // OP
      vh.set(FIELD_VALUE); // OP
      vh.set(FIELD_VALUE); // OP
      vh.set(FIELD_VALUE); // OP
      vh.set(FIELD_VALUE); // OP
      vh.set(FIELD_VALUE); // OP
      vh.set(FIELD_VALUE); // OP
      vh.set(FIELD_VALUE); // OP
      vh.set(FIELD_VALUE); // OP
      vh.set(FIELD_VALUE); // OP
      vh.set(FIELD_VALUE); // OP
      vh.set(FIELD_VALUE); // OP
      vh.set(FIELD_VALUE); // OP
      vh.set(FIELD_VALUE); // OP
      vh.set(FIELD_VALUE); // OP
      vh.set(FIELD_VALUE); // OP
      vh.set(FIELD_VALUE); // OP
      vh.set(FIELD_VALUE); // OP
      vh.set(FIELD_VALUE); // OP
    }
  }

  @Override
  public int innerIterations() {
      return 3000;
  }
}

class VarHandleSetFieldIntBenchmark extends MicroBenchmark {
  static final int FIELD_VALUE = 42;
  int field = ~FIELD_VALUE;
  VarHandle vh;

  VarHandleSetFieldIntBenchmark() throws Throwable {
    vh = MethodHandles.lookup().findVarHandle(this.getClass(), "field", int.class);
  }

  @Override
  public void teardown() {
    if (field != FIELD_VALUE) {
      throw new RuntimeException("field has unexpected value: " + field);
    }
  }

  @Override
  public void run() {
    for (int pass = 0; pass < 100; ++pass) {
      vh.set(this, FIELD_VALUE); // OP
      vh.set(this, FIELD_VALUE); // OP
      vh.set(this, FIELD_VALUE); // OP
      vh.set(this, FIELD_VALUE); // OP
      vh.set(this, FIELD_VALUE); // OP
      vh.set(this, FIELD_VALUE); // OP
      vh.set(this, FIELD_VALUE); // OP
      vh.set(this, FIELD_VALUE); // OP
      vh.set(this, FIELD_VALUE); // OP
      vh.set(this, FIELD_VALUE); // OP
      vh.set(this, FIELD_VALUE); // OP
      vh.set(this, FIELD_VALUE); // OP
      vh.set(this, FIELD_VALUE); // OP
      vh.set(this, FIELD_VALUE); // OP
      vh.set(this, FIELD_VALUE); // OP
      vh.set(this, FIELD_VALUE); // OP
      vh.set(this, FIELD_VALUE); // OP
      vh.set(this, FIELD_VALUE); // OP
      vh.set(this, FIELD_VALUE); // OP
      vh.set(this, FIELD_VALUE); // OP
      vh.set(this, FIELD_VALUE); // OP
      vh.set(this, FIELD_VALUE); // OP
      vh.set(this, FIELD_VALUE); // OP
      vh.set(this, FIELD_VALUE); // OP
      vh.set(this, FIELD_VALUE); // OP
      vh.set(this, FIELD_VALUE); // OP
      vh.set(this, FIELD_VALUE); // OP
      vh.set(this, FIELD_VALUE); // OP
      vh.set(this, FIELD_VALUE); // OP
      vh.set(this, FIELD_VALUE); // OP
    }
  }

  @Override
  public int innerIterations() {
      return 3000;
  }
}

class VarHandleSetFieldStringBenchmark extends MicroBenchmark {
  static final String FIELD_VALUE = "field value";
  String field = null;
  VarHandle vh;

  VarHandleSetFieldStringBenchmark() throws Throwable {
    vh = MethodHandles.lookup().findVarHandle(this.getClass(), "field", String.class);
  }

  @Override
  public void teardown() {
    if (!field.equals(FIELD_VALUE)) {
      throw new RuntimeException("field has unexpected value: " + field);
    }
  }

  @Override
  public void run() {
    for (int pass = 0; pass < 100; ++pass) {
      vh.set(this, FIELD_VALUE); // OP
      vh.set(this, FIELD_VALUE); // OP
      vh.set(this, FIELD_VALUE); // OP
      vh.set(this, FIELD_VALUE); // OP
      vh.set(this, FIELD_VALUE); // OP
      vh.set(this, FIELD_VALUE); // OP
      vh.set(this, FIELD_VALUE); // OP
      vh.set(this, FIELD_VALUE); // OP
      vh.set(this, FIELD_VALUE); // OP
      vh.set(this, FIELD_VALUE); // OP
      vh.set(this, FIELD_VALUE); // OP
      vh.set(this, FIELD_VALUE); // OP
      vh.set(this, FIELD_VALUE); // OP
      vh.set(this, FIELD_VALUE); // OP
      vh.set(this, FIELD_VALUE); // OP
      vh.set(this, FIELD_VALUE); // OP
      vh.set(this, FIELD_VALUE); // OP
      vh.set(this, FIELD_VALUE); // OP
      vh.set(this, FIELD_VALUE); // OP
      vh.set(this, FIELD_VALUE); // OP
      vh.set(this, FIELD_VALUE); // OP
      vh.set(this, FIELD_VALUE); // OP
      vh.set(this, FIELD_VALUE); // OP
      vh.set(this, FIELD_VALUE); // OP
      vh.set(this, FIELD_VALUE); // OP
      vh.set(this, FIELD_VALUE); // OP
      vh.set(this, FIELD_VALUE); // OP
      vh.set(this, FIELD_VALUE); // OP
      vh.set(this, FIELD_VALUE); // OP
      vh.set(this, FIELD_VALUE); // OP
    }
  }

  @Override
  public int innerIterations() {
      return 3000;
  }
}

class VarHandleSetVolatileStaticFieldIntBenchmark extends MicroBenchmark {
  static final int FIELD_VALUE = 42;
  static int field = ~FIELD_VALUE;

  VarHandle vh;

  VarHandleSetVolatileStaticFieldIntBenchmark() throws Throwable {
    vh = MethodHandles.lookup().findStaticVarHandle(this.getClass(), "field", int.class);
  }

  @Override
  public void teardown() {
    if (field != FIELD_VALUE) {
      throw new RuntimeException("field has unexpected value: " + field);
    }
  }

  @Override
  public void run() {
    for (int pass = 0; pass < 100; ++pass) {
      vh.setVolatile(FIELD_VALUE); // OP
      vh.setVolatile(FIELD_VALUE); // OP
      vh.setVolatile(FIELD_VALUE); // OP
      vh.setVolatile(FIELD_VALUE); // OP
      vh.setVolatile(FIELD_VALUE); // OP
      vh.setVolatile(FIELD_VALUE); // OP
      vh.setVolatile(FIELD_VALUE); // OP
      vh.setVolatile(FIELD_VALUE); // OP
      vh.setVolatile(FIELD_VALUE); // OP
      vh.setVolatile(FIELD_VALUE); // OP
      vh.setVolatile(FIELD_VALUE); // OP
      vh.setVolatile(FIELD_VALUE); // OP
      vh.setVolatile(FIELD_VALUE); // OP
      vh.setVolatile(FIELD_VALUE); // OP
      vh.setVolatile(FIELD_VALUE); // OP
      vh.setVolatile(FIELD_VALUE); // OP
      vh.setVolatile(FIELD_VALUE); // OP
      vh.setVolatile(FIELD_VALUE); // OP
      vh.setVolatile(FIELD_VALUE); // OP
      vh.setVolatile(FIELD_VALUE); // OP
      vh.setVolatile(FIELD_VALUE); // OP
      vh.setVolatile(FIELD_VALUE); // OP
      vh.setVolatile(FIELD_VALUE); // OP
      vh.setVolatile(FIELD_VALUE); // OP
      vh.setVolatile(FIELD_VALUE); // OP
      vh.setVolatile(FIELD_VALUE); // OP
      vh.setVolatile(FIELD_VALUE); // OP
      vh.setVolatile(FIELD_VALUE); // OP
      vh.setVolatile(FIELD_VALUE); // OP
      vh.setVolatile(FIELD_VALUE); // OP
    }
  }

  @Override
  public int innerIterations() {
      return 3000;
  }
}

class VarHandleSetVolatileStaticFieldStringBenchmark extends MicroBenchmark {
  static final String FIELD_VALUE = "a string";
  static String field = null;

  VarHandle vh;

  VarHandleSetVolatileStaticFieldStringBenchmark() throws Throwable {
    vh = MethodHandles.lookup().findStaticVarHandle(this.getClass(), "field", String.class);
  }

  @Override
  public void teardown() {
    if (!field.equals(FIELD_VALUE)) {
      throw new RuntimeException("field has unexpected value: " + field);
    }
  }

  @Override
  public void run() {
    for (int pass = 0; pass < 100; ++pass) {
      vh.setVolatile(FIELD_VALUE); // OP
      vh.setVolatile(FIELD_VALUE); // OP
      vh.setVolatile(FIELD_VALUE); // OP
      vh.setVolatile(FIELD_VALUE); // OP
      vh.setVolatile(FIELD_VALUE); // OP
      vh.setVolatile(FIELD_VALUE); // OP
      vh.setVolatile(FIELD_VALUE); // OP
      vh.setVolatile(FIELD_VALUE); // OP
      vh.setVolatile(FIELD_VALUE); // OP
      vh.setVolatile(FIELD_VALUE); // OP
      vh.setVolatile(FIELD_VALUE); // OP
      vh.setVolatile(FIELD_VALUE); // OP
      vh.setVolatile(FIELD_VALUE); // OP
      vh.setVolatile(FIELD_VALUE); // OP
      vh.setVolatile(FIELD_VALUE); // OP
      vh.setVolatile(FIELD_VALUE); // OP
      vh.setVolatile(FIELD_VALUE); // OP
      vh.setVolatile(FIELD_VALUE); // OP
      vh.setVolatile(FIELD_VALUE); // OP
      vh.setVolatile(FIELD_VALUE); // OP
      vh.setVolatile(FIELD_VALUE); // OP
      vh.setVolatile(FIELD_VALUE); // OP
      vh.setVolatile(FIELD_VALUE); // OP
      vh.setVolatile(FIELD_VALUE); // OP
      vh.setVolatile(FIELD_VALUE); // OP
      vh.setVolatile(FIELD_VALUE); // OP
      vh.setVolatile(FIELD_VALUE); // OP
      vh.setVolatile(FIELD_VALUE); // OP
      vh.setVolatile(FIELD_VALUE); // OP
      vh.setVolatile(FIELD_VALUE); // OP
    }
  }

  @Override
  public int innerIterations() {
      return 3000;
  }
}

class VarHandleSetVolatileFieldIntBenchmark extends MicroBenchmark {
  static final int FIELD_VALUE = 42;
  int field = ~FIELD_VALUE;
  VarHandle vh;

  VarHandleSetVolatileFieldIntBenchmark() throws Throwable {
    vh = MethodHandles.lookup().findVarHandle(this.getClass(), "field", int.class);
  }

  @Override
  public void teardown() {
    if (field != FIELD_VALUE) {
      throw new RuntimeException("field has unexpected value: " + field);
    }
  }

  @Override
  public void run() {
    for (int pass = 0; pass < 100; ++pass) {
      vh.setVolatile(this, FIELD_VALUE); // OP
      vh.setVolatile(this, FIELD_VALUE); // OP
      vh.setVolatile(this, FIELD_VALUE); // OP
      vh.setVolatile(this, FIELD_VALUE); // OP
      vh.setVolatile(this, FIELD_VALUE); // OP
      vh.setVolatile(this, FIELD_VALUE); // OP
      vh.setVolatile(this, FIELD_VALUE); // OP
      vh.setVolatile(this, FIELD_VALUE); // OP
      vh.setVolatile(this, FIELD_VALUE); // OP
      vh.setVolatile(this, FIELD_VALUE); // OP
      vh.setVolatile(this, FIELD_VALUE); // OP
      vh.setVolatile(this, FIELD_VALUE); // OP
      vh.setVolatile(this, FIELD_VALUE); // OP
      vh.setVolatile(this, FIELD_VALUE); // OP
      vh.setVolatile(this, FIELD_VALUE); // OP
      vh.setVolatile(this, FIELD_VALUE); // OP
      vh.setVolatile(this, FIELD_VALUE); // OP
      vh.setVolatile(this, FIELD_VALUE); // OP
      vh.setVolatile(this, FIELD_VALUE); // OP
      vh.setVolatile(this, FIELD_VALUE); // OP
      vh.setVolatile(this, FIELD_VALUE); // OP
      vh.setVolatile(this, FIELD_VALUE); // OP
      vh.setVolatile(this, FIELD_VALUE); // OP
      vh.setVolatile(this, FIELD_VALUE); // OP
      vh.setVolatile(this, FIELD_VALUE); // OP
      vh.setVolatile(this, FIELD_VALUE); // OP
      vh.setVolatile(this, FIELD_VALUE); // OP
      vh.setVolatile(this, FIELD_VALUE); // OP
      vh.setVolatile(this, FIELD_VALUE); // OP
      vh.setVolatile(this, FIELD_VALUE); // OP
    }
  }

  @Override
  public int innerIterations() {
      return 3000;
  }
}

class VarHandleSetVolatileFieldStringBenchmark extends MicroBenchmark {
  static final String FIELD_VALUE = "field value";
  String field = null;
  VarHandle vh;

  VarHandleSetVolatileFieldStringBenchmark() throws Throwable {
    vh = MethodHandles.lookup().findVarHandle(this.getClass(), "field", String.class);
  }

  @Override
  public void teardown() {
    if (!field.equals(FIELD_VALUE)) {
      throw new RuntimeException("field has unexpected value: " + field);
    }
  }

  @Override
  public void run() {
    for (int pass = 0; pass < 100; ++pass) {
      vh.setVolatile(this, FIELD_VALUE); // OP
      vh.setVolatile(this, FIELD_VALUE); // OP
      vh.setVolatile(this, FIELD_VALUE); // OP
      vh.setVolatile(this, FIELD_VALUE); // OP
      vh.setVolatile(this, FIELD_VALUE); // OP
      vh.setVolatile(this, FIELD_VALUE); // OP
      vh.setVolatile(this, FIELD_VALUE); // OP
      vh.setVolatile(this, FIELD_VALUE); // OP
      vh.setVolatile(this, FIELD_VALUE); // OP
      vh.setVolatile(this, FIELD_VALUE); // OP
      vh.setVolatile(this, FIELD_VALUE); // OP
      vh.setVolatile(this, FIELD_VALUE); // OP
      vh.setVolatile(this, FIELD_VALUE); // OP
      vh.setVolatile(this, FIELD_VALUE); // OP
      vh.setVolatile(this, FIELD_VALUE); // OP
      vh.setVolatile(this, FIELD_VALUE); // OP
      vh.setVolatile(this, FIELD_VALUE); // OP
      vh.setVolatile(this, FIELD_VALUE); // OP
      vh.setVolatile(this, FIELD_VALUE); // OP
      vh.setVolatile(this, FIELD_VALUE); // OP
      vh.setVolatile(this, FIELD_VALUE); // OP
      vh.setVolatile(this, FIELD_VALUE); // OP
      vh.setVolatile(this, FIELD_VALUE); // OP
      vh.setVolatile(this, FIELD_VALUE); // OP
      vh.setVolatile(this, FIELD_VALUE); // OP
      vh.setVolatile(this, FIELD_VALUE); // OP
      vh.setVolatile(this, FIELD_VALUE); // OP
      vh.setVolatile(this, FIELD_VALUE); // OP
      vh.setVolatile(this, FIELD_VALUE); // OP
      vh.setVolatile(this, FIELD_VALUE); // OP
    }
  }

  @Override
  public int innerIterations() {
      return 3000;
  }
}

class VarHandleSetOpaqueStaticFieldIntBenchmark extends MicroBenchmark {
  static final int FIELD_VALUE = 42;
  static int field = ~FIELD_VALUE;

  VarHandle vh;

  VarHandleSetOpaqueStaticFieldIntBenchmark() throws Throwable {
    vh = MethodHandles.lookup().findStaticVarHandle(this.getClass(), "field", int.class);
  }

  @Override
  public void teardown() {
    if (field != FIELD_VALUE) {
      throw new RuntimeException("field has unexpected value: " + field);
    }
  }

  @Override
  public void run() {
    for (int pass = 0; pass < 100; ++pass) {
      vh.setOpaque(FIELD_VALUE); // OP
      vh.setOpaque(FIELD_VALUE); // OP
      vh.setOpaque(FIELD_VALUE); // OP
      vh.setOpaque(FIELD_VALUE); // OP
      vh.setOpaque(FIELD_VALUE); // OP
      vh.setOpaque(FIELD_VALUE); // OP
      vh.setOpaque(FIELD_VALUE); // OP
      vh.setOpaque(FIELD_VALUE); // OP
      vh.setOpaque(FIELD_VALUE); // OP
      vh.setOpaque(FIELD_VALUE); // OP
      vh.setOpaque(FIELD_VALUE); // OP
      vh.setOpaque(FIELD_VALUE); // OP
      vh.setOpaque(FIELD_VALUE); // OP
      vh.setOpaque(FIELD_VALUE); // OP
      vh.setOpaque(FIELD_VALUE); // OP
      vh.setOpaque(FIELD_VALUE); // OP
      vh.setOpaque(FIELD_VALUE); // OP
      vh.setOpaque(FIELD_VALUE); // OP
      vh.setOpaque(FIELD_VALUE); // OP
      vh.setOpaque(FIELD_VALUE); // OP
      vh.setOpaque(FIELD_VALUE); // OP
      vh.setOpaque(FIELD_VALUE); // OP
      vh.setOpaque(FIELD_VALUE); // OP
      vh.setOpaque(FIELD_VALUE); // OP
      vh.setOpaque(FIELD_VALUE); // OP
      vh.setOpaque(FIELD_VALUE); // OP
      vh.setOpaque(FIELD_VALUE); // OP
      vh.setOpaque(FIELD_VALUE); // OP
      vh.setOpaque(FIELD_VALUE); // OP
      vh.setOpaque(FIELD_VALUE); // OP
    }
  }

  @Override
  public int innerIterations() {
      return 3000;
  }
}

class VarHandleSetOpaqueStaticFieldStringBenchmark extends MicroBenchmark {
  static final String FIELD_VALUE = "a string";
  static String field = null;

  VarHandle vh;

  VarHandleSetOpaqueStaticFieldStringBenchmark() throws Throwable {
    vh = MethodHandles.lookup().findStaticVarHandle(this.getClass(), "field", String.class);
  }

  @Override
  public void teardown() {
    if (!field.equals(FIELD_VALUE)) {
      throw new RuntimeException("field has unexpected value: " + field);
    }
  }

  @Override
  public void run() {
    for (int pass = 0; pass < 100; ++pass) {
      vh.setOpaque(FIELD_VALUE); // OP
      vh.setOpaque(FIELD_VALUE); // OP
      vh.setOpaque(FIELD_VALUE); // OP
      vh.setOpaque(FIELD_VALUE); // OP
      vh.setOpaque(FIELD_VALUE); // OP
      vh.setOpaque(FIELD_VALUE); // OP
      vh.setOpaque(FIELD_VALUE); // OP
      vh.setOpaque(FIELD_VALUE); // OP
      vh.setOpaque(FIELD_VALUE); // OP
      vh.setOpaque(FIELD_VALUE); // OP
      vh.setOpaque(FIELD_VALUE); // OP
      vh.setOpaque(FIELD_VALUE); // OP
      vh.setOpaque(FIELD_VALUE); // OP
      vh.setOpaque(FIELD_VALUE); // OP
      vh.setOpaque(FIELD_VALUE); // OP
      vh.setOpaque(FIELD_VALUE); // OP
      vh.setOpaque(FIELD_VALUE); // OP
      vh.setOpaque(FIELD_VALUE); // OP
      vh.setOpaque(FIELD_VALUE); // OP
      vh.setOpaque(FIELD_VALUE); // OP
      vh.setOpaque(FIELD_VALUE); // OP
      vh.setOpaque(FIELD_VALUE); // OP
      vh.setOpaque(FIELD_VALUE); // OP
      vh.setOpaque(FIELD_VALUE); // OP
      vh.setOpaque(FIELD_VALUE); // OP
      vh.setOpaque(FIELD_VALUE); // OP
      vh.setOpaque(FIELD_VALUE); // OP
      vh.setOpaque(FIELD_VALUE); // OP
      vh.setOpaque(FIELD_VALUE); // OP
      vh.setOpaque(FIELD_VALUE); // OP
    }
  }

  @Override
  public int innerIterations() {
      return 3000;
  }
}

class VarHandleSetOpaqueFieldIntBenchmark extends MicroBenchmark {
  static final int FIELD_VALUE = 42;
  int field = ~FIELD_VALUE;
  VarHandle vh;

  VarHandleSetOpaqueFieldIntBenchmark() throws Throwable {
    vh = MethodHandles.lookup().findVarHandle(this.getClass(), "field", int.class);
  }

  @Override
  public void teardown() {
    if (field != FIELD_VALUE) {
      throw new RuntimeException("field has unexpected value: " + field);
    }
  }

  @Override
  public void run() {
    for (int pass = 0; pass < 100; ++pass) {
      vh.setOpaque(this, FIELD_VALUE); // OP
      vh.setOpaque(this, FIELD_VALUE); // OP
      vh.setOpaque(this, FIELD_VALUE); // OP
      vh.setOpaque(this, FIELD_VALUE); // OP
      vh.setOpaque(this, FIELD_VALUE); // OP
      vh.setOpaque(this, FIELD_VALUE); // OP
      vh.setOpaque(this, FIELD_VALUE); // OP
      vh.setOpaque(this, FIELD_VALUE); // OP
      vh.setOpaque(this, FIELD_VALUE); // OP
      vh.setOpaque(this, FIELD_VALUE); // OP
      vh.setOpaque(this, FIELD_VALUE); // OP
      vh.setOpaque(this, FIELD_VALUE); // OP
      vh.setOpaque(this, FIELD_VALUE); // OP
      vh.setOpaque(this, FIELD_VALUE); // OP
      vh.setOpaque(this, FIELD_VALUE); // OP
      vh.setOpaque(this, FIELD_VALUE); // OP
      vh.setOpaque(this, FIELD_VALUE); // OP
      vh.setOpaque(this, FIELD_VALUE); // OP
      vh.setOpaque(this, FIELD_VALUE); // OP
      vh.setOpaque(this, FIELD_VALUE); // OP
      vh.setOpaque(this, FIELD_VALUE); // OP
      vh.setOpaque(this, FIELD_VALUE); // OP
      vh.setOpaque(this, FIELD_VALUE); // OP
      vh.setOpaque(this, FIELD_VALUE); // OP
      vh.setOpaque(this, FIELD_VALUE); // OP
      vh.setOpaque(this, FIELD_VALUE); // OP
      vh.setOpaque(this, FIELD_VALUE); // OP
      vh.setOpaque(this, FIELD_VALUE); // OP
      vh.setOpaque(this, FIELD_VALUE); // OP
      vh.setOpaque(this, FIELD_VALUE); // OP
    }
  }

  @Override
  public int innerIterations() {
      return 3000;
  }
}

class VarHandleSetOpaqueFieldStringBenchmark extends MicroBenchmark {
  static final String FIELD_VALUE = "field value";
  String field = null;
  VarHandle vh;

  VarHandleSetOpaqueFieldStringBenchmark() throws Throwable {
    vh = MethodHandles.lookup().findVarHandle(this.getClass(), "field", String.class);
  }

  @Override
  public void teardown() {
    if (!field.equals(FIELD_VALUE)) {
      throw new RuntimeException("field has unexpected value: " + field);
    }
  }

  @Override
  public void run() {
    for (int pass = 0; pass < 100; ++pass) {
      vh.setOpaque(this, FIELD_VALUE); // OP
      vh.setOpaque(this, FIELD_VALUE); // OP
      vh.setOpaque(this, FIELD_VALUE); // OP
      vh.setOpaque(this, FIELD_VALUE); // OP
      vh.setOpaque(this, FIELD_VALUE); // OP
      vh.setOpaque(this, FIELD_VALUE); // OP
      vh.setOpaque(this, FIELD_VALUE); // OP
      vh.setOpaque(this, FIELD_VALUE); // OP
      vh.setOpaque(this, FIELD_VALUE); // OP
      vh.setOpaque(this, FIELD_VALUE); // OP
      vh.setOpaque(this, FIELD_VALUE); // OP
      vh.setOpaque(this, FIELD_VALUE); // OP
      vh.setOpaque(this, FIELD_VALUE); // OP
      vh.setOpaque(this, FIELD_VALUE); // OP
      vh.setOpaque(this, FIELD_VALUE); // OP
      vh.setOpaque(this, FIELD_VALUE); // OP
      vh.setOpaque(this, FIELD_VALUE); // OP
      vh.setOpaque(this, FIELD_VALUE); // OP
      vh.setOpaque(this, FIELD_VALUE); // OP
      vh.setOpaque(this, FIELD_VALUE); // OP
      vh.setOpaque(this, FIELD_VALUE); // OP
      vh.setOpaque(this, FIELD_VALUE); // OP
      vh.setOpaque(this, FIELD_VALUE); // OP
      vh.setOpaque(this, FIELD_VALUE); // OP
      vh.setOpaque(this, FIELD_VALUE); // OP
      vh.setOpaque(this, FIELD_VALUE); // OP
      vh.setOpaque(this, FIELD_VALUE); // OP
      vh.setOpaque(this, FIELD_VALUE); // OP
      vh.setOpaque(this, FIELD_VALUE); // OP
      vh.setOpaque(this, FIELD_VALUE); // OP
    }
  }

  @Override
  public int innerIterations() {
      return 3000;
  }
}

class VarHandleSetReleaseStaticFieldIntBenchmark extends MicroBenchmark {
  static final int FIELD_VALUE = 42;
  static int field = ~FIELD_VALUE;

  VarHandle vh;

  VarHandleSetReleaseStaticFieldIntBenchmark() throws Throwable {
    vh = MethodHandles.lookup().findStaticVarHandle(this.getClass(), "field", int.class);
  }

  @Override
  public void teardown() {
    if (field != FIELD_VALUE) {
      throw new RuntimeException("field has unexpected value: " + field);
    }
  }

  @Override
  public void run() {
    for (int pass = 0; pass < 100; ++pass) {
      vh.setRelease(FIELD_VALUE); // OP
      vh.setRelease(FIELD_VALUE); // OP
      vh.setRelease(FIELD_VALUE); // OP
      vh.setRelease(FIELD_VALUE); // OP
      vh.setRelease(FIELD_VALUE); // OP
      vh.setRelease(FIELD_VALUE); // OP
      vh.setRelease(FIELD_VALUE); // OP
      vh.setRelease(FIELD_VALUE); // OP
      vh.setRelease(FIELD_VALUE); // OP
      vh.setRelease(FIELD_VALUE); // OP
      vh.setRelease(FIELD_VALUE); // OP
      vh.setRelease(FIELD_VALUE); // OP
      vh.setRelease(FIELD_VALUE); // OP
      vh.setRelease(FIELD_VALUE); // OP
      vh.setRelease(FIELD_VALUE); // OP
      vh.setRelease(FIELD_VALUE); // OP
      vh.setRelease(FIELD_VALUE); // OP
      vh.setRelease(FIELD_VALUE); // OP
      vh.setRelease(FIELD_VALUE); // OP
      vh.setRelease(FIELD_VALUE); // OP
      vh.setRelease(FIELD_VALUE); // OP
      vh.setRelease(FIELD_VALUE); // OP
      vh.setRelease(FIELD_VALUE); // OP
      vh.setRelease(FIELD_VALUE); // OP
      vh.setRelease(FIELD_VALUE); // OP
      vh.setRelease(FIELD_VALUE); // OP
      vh.setRelease(FIELD_VALUE); // OP
      vh.setRelease(FIELD_VALUE); // OP
      vh.setRelease(FIELD_VALUE); // OP
      vh.setRelease(FIELD_VALUE); // OP
    }
  }

  @Override
  public int innerIterations() {
      return 3000;
  }
}

class VarHandleSetReleaseStaticFieldStringBenchmark extends MicroBenchmark {
  static final String FIELD_VALUE = "a string";
  static String field = null;

  VarHandle vh;

  VarHandleSetReleaseStaticFieldStringBenchmark() throws Throwable {
    vh = MethodHandles.lookup().findStaticVarHandle(this.getClass(), "field", String.class);
  }

  @Override
  public void teardown() {
    if (!field.equals(FIELD_VALUE)) {
      throw new RuntimeException("field has unexpected value: " + field);
    }
  }

  @Override
  public void run() {
    for (int pass = 0; pass < 100; ++pass) {
      vh.setRelease(FIELD_VALUE); // OP
      vh.setRelease(FIELD_VALUE); // OP
      vh.setRelease(FIELD_VALUE); // OP
      vh.setRelease(FIELD_VALUE); // OP
      vh.setRelease(FIELD_VALUE); // OP
      vh.setRelease(FIELD_VALUE); // OP
      vh.setRelease(FIELD_VALUE); // OP
      vh.setRelease(FIELD_VALUE); // OP
      vh.setRelease(FIELD_VALUE); // OP
      vh.setRelease(FIELD_VALUE); // OP
      vh.setRelease(FIELD_VALUE); // OP
      vh.setRelease(FIELD_VALUE); // OP
      vh.setRelease(FIELD_VALUE); // OP
      vh.setRelease(FIELD_VALUE); // OP
      vh.setRelease(FIELD_VALUE); // OP
      vh.setRelease(FIELD_VALUE); // OP
      vh.setRelease(FIELD_VALUE); // OP
      vh.setRelease(FIELD_VALUE); // OP
      vh.setRelease(FIELD_VALUE); // OP
      vh.setRelease(FIELD_VALUE); // OP
      vh.setRelease(FIELD_VALUE); // OP
      vh.setRelease(FIELD_VALUE); // OP
      vh.setRelease(FIELD_VALUE); // OP
      vh.setRelease(FIELD_VALUE); // OP
      vh.setRelease(FIELD_VALUE); // OP
      vh.setRelease(FIELD_VALUE); // OP
      vh.setRelease(FIELD_VALUE); // OP
      vh.setRelease(FIELD_VALUE); // OP
      vh.setRelease(FIELD_VALUE); // OP
      vh.setRelease(FIELD_VALUE); // OP
    }
  }

  @Override
  public int innerIterations() {
      return 3000;
  }
}

class VarHandleSetReleaseFieldIntBenchmark extends MicroBenchmark {
  static final int FIELD_VALUE = 42;
  int field = ~FIELD_VALUE;
  VarHandle vh;

  VarHandleSetReleaseFieldIntBenchmark() throws Throwable {
    vh = MethodHandles.lookup().findVarHandle(this.getClass(), "field", int.class);
  }

  @Override
  public void teardown() {
    if (field != FIELD_VALUE) {
      throw new RuntimeException("field has unexpected value: " + field);
    }
  }

  @Override
  public void run() {
    for (int pass = 0; pass < 100; ++pass) {
      vh.setRelease(this, FIELD_VALUE); // OP
      vh.setRelease(this, FIELD_VALUE); // OP
      vh.setRelease(this, FIELD_VALUE); // OP
      vh.setRelease(this, FIELD_VALUE); // OP
      vh.setRelease(this, FIELD_VALUE); // OP
      vh.setRelease(this, FIELD_VALUE); // OP
      vh.setRelease(this, FIELD_VALUE); // OP
      vh.setRelease(this, FIELD_VALUE); // OP
      vh.setRelease(this, FIELD_VALUE); // OP
      vh.setRelease(this, FIELD_VALUE); // OP
      vh.setRelease(this, FIELD_VALUE); // OP
      vh.setRelease(this, FIELD_VALUE); // OP
      vh.setRelease(this, FIELD_VALUE); // OP
      vh.setRelease(this, FIELD_VALUE); // OP
      vh.setRelease(this, FIELD_VALUE); // OP
      vh.setRelease(this, FIELD_VALUE); // OP
      vh.setRelease(this, FIELD_VALUE); // OP
      vh.setRelease(this, FIELD_VALUE); // OP
      vh.setRelease(this, FIELD_VALUE); // OP
      vh.setRelease(this, FIELD_VALUE); // OP
      vh.setRelease(this, FIELD_VALUE); // OP
      vh.setRelease(this, FIELD_VALUE); // OP
      vh.setRelease(this, FIELD_VALUE); // OP
      vh.setRelease(this, FIELD_VALUE); // OP
      vh.setRelease(this, FIELD_VALUE); // OP
      vh.setRelease(this, FIELD_VALUE); // OP
      vh.setRelease(this, FIELD_VALUE); // OP
      vh.setRelease(this, FIELD_VALUE); // OP
      vh.setRelease(this, FIELD_VALUE); // OP
      vh.setRelease(this, FIELD_VALUE); // OP
    }
  }

  @Override
  public int innerIterations() {
      return 3000;
  }
}

class VarHandleSetReleaseFieldStringBenchmark extends MicroBenchmark {
  static final String FIELD_VALUE = "field value";
  String field = null;
  VarHandle vh;

  VarHandleSetReleaseFieldStringBenchmark() throws Throwable {
    vh = MethodHandles.lookup().findVarHandle(this.getClass(), "field", String.class);
  }

  @Override
  public void teardown() {
    if (!field.equals(FIELD_VALUE)) {
      throw new RuntimeException("field has unexpected value: " + field);
    }
  }

  @Override
  public void run() {
    for (int pass = 0; pass < 100; ++pass) {
      vh.setRelease(this, FIELD_VALUE); // OP
      vh.setRelease(this, FIELD_VALUE); // OP
      vh.setRelease(this, FIELD_VALUE); // OP
      vh.setRelease(this, FIELD_VALUE); // OP
      vh.setRelease(this, FIELD_VALUE); // OP
      vh.setRelease(this, FIELD_VALUE); // OP
      vh.setRelease(this, FIELD_VALUE); // OP
      vh.setRelease(this, FIELD_VALUE); // OP
      vh.setRelease(this, FIELD_VALUE); // OP
      vh.setRelease(this, FIELD_VALUE); // OP
      vh.setRelease(this, FIELD_VALUE); // OP
      vh.setRelease(this, FIELD_VALUE); // OP
      vh.setRelease(this, FIELD_VALUE); // OP
      vh.setRelease(this, FIELD_VALUE); // OP
      vh.setRelease(this, FIELD_VALUE); // OP
      vh.setRelease(this, FIELD_VALUE); // OP
      vh.setRelease(this, FIELD_VALUE); // OP
      vh.setRelease(this, FIELD_VALUE); // OP
      vh.setRelease(this, FIELD_VALUE); // OP
      vh.setRelease(this, FIELD_VALUE); // OP
      vh.setRelease(this, FIELD_VALUE); // OP
      vh.setRelease(this, FIELD_VALUE); // OP
      vh.setRelease(this, FIELD_VALUE); // OP
      vh.setRelease(this, FIELD_VALUE); // OP
      vh.setRelease(this, FIELD_VALUE); // OP
      vh.setRelease(this, FIELD_VALUE); // OP
      vh.setRelease(this, FIELD_VALUE); // OP
      vh.setRelease(this, FIELD_VALUE); // OP
      vh.setRelease(this, FIELD_VALUE); // OP
      vh.setRelease(this, FIELD_VALUE); // OP
    }
  }

  @Override
  public int innerIterations() {
      return 3000;
  }
}

class VarHandleCompareAndSetStaticFieldIntBenchmark extends MicroBenchmark {
  static final int FIELD_VALUE = 42;
  static int field = ~FIELD_VALUE;

  VarHandle vh;

  VarHandleCompareAndSetStaticFieldIntBenchmark() throws Throwable {
    vh = MethodHandles.lookup().findStaticVarHandle(this.getClass(), "field", int.class);
  }

  @Override
  public void run() {
    boolean success;
    for (int pass = 0; pass < 100; ++pass) {
      success = vh.compareAndSet(field, ~field); // OP
      success = vh.compareAndSet(field, ~field); // OP
      success = vh.compareAndSet(field, ~field); // OP
      success = vh.compareAndSet(field, ~field); // OP
      success = vh.compareAndSet(field, ~field); // OP
      success = vh.compareAndSet(field, ~field); // OP
      success = vh.compareAndSet(field, ~field); // OP
      success = vh.compareAndSet(field, ~field); // OP
      success = vh.compareAndSet(field, ~field); // OP
      success = vh.compareAndSet(field, ~field); // OP
      success = vh.compareAndSet(field, ~field); // OP
      success = vh.compareAndSet(field, ~field); // OP
      success = vh.compareAndSet(field, ~field); // OP
      success = vh.compareAndSet(field, ~field); // OP
      success = vh.compareAndSet(field, ~field); // OP
      success = vh.compareAndSet(field, ~field); // OP
      success = vh.compareAndSet(field, ~field); // OP
      success = vh.compareAndSet(field, ~field); // OP
      success = vh.compareAndSet(field, ~field); // OP
      success = vh.compareAndSet(field, ~field); // OP
      success = vh.compareAndSet(field, ~field); // OP
      success = vh.compareAndSet(field, ~field); // OP
      success = vh.compareAndSet(field, ~field); // OP
      success = vh.compareAndSet(field, ~field); // OP
      success = vh.compareAndSet(field, ~field); // OP
      success = vh.compareAndSet(field, ~field); // OP
      success = vh.compareAndSet(field, ~field); // OP
      success = vh.compareAndSet(field, ~field); // OP
      success = vh.compareAndSet(field, ~field); // OP
      success = vh.compareAndSet(field, ~field); // OP
    }
  }

  @Override
  public int innerIterations() {
      return 3000;
  }
}

class VarHandleCompareAndSetStaticFieldStringBenchmark extends MicroBenchmark {
  static final String FIELD_VALUE = "a string";
  static String field = null;

  VarHandle vh;

  VarHandleCompareAndSetStaticFieldStringBenchmark() throws Throwable {
    vh = MethodHandles.lookup().findStaticVarHandle(this.getClass(), "field", String.class);
  }

  @Override
  public void run() {
    String newValue = (field != null) ? null : FIELD_VALUE;
    boolean success;
    for (int pass = 0; pass < 100; ++pass) {
      success = vh.compareAndSet(field, newValue); // OP
      success = vh.compareAndSet(field, newValue); // OP
      success = vh.compareAndSet(field, newValue); // OP
      success = vh.compareAndSet(field, newValue); // OP
      success = vh.compareAndSet(field, newValue); // OP
      success = vh.compareAndSet(field, newValue); // OP
      success = vh.compareAndSet(field, newValue); // OP
      success = vh.compareAndSet(field, newValue); // OP
      success = vh.compareAndSet(field, newValue); // OP
      success = vh.compareAndSet(field, newValue); // OP
      success = vh.compareAndSet(field, newValue); // OP
      success = vh.compareAndSet(field, newValue); // OP
      success = vh.compareAndSet(field, newValue); // OP
      success = vh.compareAndSet(field, newValue); // OP
      success = vh.compareAndSet(field, newValue); // OP
      success = vh.compareAndSet(field, newValue); // OP
      success = vh.compareAndSet(field, newValue); // OP
      success = vh.compareAndSet(field, newValue); // OP
      success = vh.compareAndSet(field, newValue); // OP
      success = vh.compareAndSet(field, newValue); // OP
      success = vh.compareAndSet(field, newValue); // OP
      success = vh.compareAndSet(field, newValue); // OP
      success = vh.compareAndSet(field, newValue); // OP
      success = vh.compareAndSet(field, newValue); // OP
      success = vh.compareAndSet(field, newValue); // OP
      success = vh.compareAndSet(field, newValue); // OP
      success = vh.compareAndSet(field, newValue); // OP
      success = vh.compareAndSet(field, newValue); // OP
      success = vh.compareAndSet(field, newValue); // OP
      success = vh.compareAndSet(field, newValue); // OP
    }
  }

  @Override
  public int innerIterations() {
      return 3000;
  }
}

class VarHandleCompareAndSetFieldIntBenchmark extends MicroBenchmark {
  static final int FIELD_VALUE = 42;

  int field = FIELD_VALUE;
  VarHandle vh;

  VarHandleCompareAndSetFieldIntBenchmark() throws Throwable {
    vh = MethodHandles.lookup().findVarHandle(this.getClass(), "field", int.class);
  }

  @Override
  public void run() {
    boolean success;
    for (int pass = 0; pass < 100; ++pass) {
      success = vh.compareAndSet(this, field, ~field); // OP
      success = vh.compareAndSet(this, field, ~field); // OP
      success = vh.compareAndSet(this, field, ~field); // OP
      success = vh.compareAndSet(this, field, ~field); // OP
      success = vh.compareAndSet(this, field, ~field); // OP
      success = vh.compareAndSet(this, field, ~field); // OP
      success = vh.compareAndSet(this, field, ~field); // OP
      success = vh.compareAndSet(this, field, ~field); // OP
      success = vh.compareAndSet(this, field, ~field); // OP
      success = vh.compareAndSet(this, field, ~field); // OP
      success = vh.compareAndSet(this, field, ~field); // OP
      success = vh.compareAndSet(this, field, ~field); // OP
      success = vh.compareAndSet(this, field, ~field); // OP
      success = vh.compareAndSet(this, field, ~field); // OP
      success = vh.compareAndSet(this, field, ~field); // OP
      success = vh.compareAndSet(this, field, ~field); // OP
      success = vh.compareAndSet(this, field, ~field); // OP
      success = vh.compareAndSet(this, field, ~field); // OP
      success = vh.compareAndSet(this, field, ~field); // OP
      success = vh.compareAndSet(this, field, ~field); // OP
      success = vh.compareAndSet(this, field, ~field); // OP
      success = vh.compareAndSet(this, field, ~field); // OP
      success = vh.compareAndSet(this, field, ~field); // OP
      success = vh.compareAndSet(this, field, ~field); // OP
      success = vh.compareAndSet(this, field, ~field); // OP
      success = vh.compareAndSet(this, field, ~field); // OP
      success = vh.compareAndSet(this, field, ~field); // OP
      success = vh.compareAndSet(this, field, ~field); // OP
      success = vh.compareAndSet(this, field, ~field); // OP
      success = vh.compareAndSet(this, field, ~field); // OP
    }
  }

  @Override
  public int innerIterations() {
      return 3000;
  }
}

class VarHandleCompareAndSetFieldStringBenchmark extends MicroBenchmark {
  static final String FIELD_VALUE = "field value";
  String field = null;
  VarHandle vh;

  VarHandleCompareAndSetFieldStringBenchmark() throws Throwable {
    vh = MethodHandles.lookup().findVarHandle(this.getClass(), "field", String.class);
  }

  @Override
  public void run() {
    String newValue = (field != null) ? null : FIELD_VALUE;
    boolean success;
    for (int pass = 0; pass < 100; ++pass) {
      success = vh.compareAndSet(this, field, newValue); // OP
      success = vh.compareAndSet(this, field, newValue); // OP
      success = vh.compareAndSet(this, field, newValue); // OP
      success = vh.compareAndSet(this, field, newValue); // OP
      success = vh.compareAndSet(this, field, newValue); // OP
      success = vh.compareAndSet(this, field, newValue); // OP
      success = vh.compareAndSet(this, field, newValue); // OP
      success = vh.compareAndSet(this, field, newValue); // OP
      success = vh.compareAndSet(this, field, newValue); // OP
      success = vh.compareAndSet(this, field, newValue); // OP
      success = vh.compareAndSet(this, field, newValue); // OP
      success = vh.compareAndSet(this, field, newValue); // OP
      success = vh.compareAndSet(this, field, newValue); // OP
      success = vh.compareAndSet(this, field, newValue); // OP
      success = vh.compareAndSet(this, field, newValue); // OP
      success = vh.compareAndSet(this, field, newValue); // OP
      success = vh.compareAndSet(this, field, newValue); // OP
      success = vh.compareAndSet(this, field, newValue); // OP
      success = vh.compareAndSet(this, field, newValue); // OP
      success = vh.compareAndSet(this, field, newValue); // OP
      success = vh.compareAndSet(this, field, newValue); // OP
      success = vh.compareAndSet(this, field, newValue); // OP
      success = vh.compareAndSet(this, field, newValue); // OP
      success = vh.compareAndSet(this, field, newValue); // OP
      success = vh.compareAndSet(this, field, newValue); // OP
      success = vh.compareAndSet(this, field, newValue); // OP
      success = vh.compareAndSet(this, field, newValue); // OP
      success = vh.compareAndSet(this, field, newValue); // OP
      success = vh.compareAndSet(this, field, newValue); // OP
      success = vh.compareAndSet(this, field, newValue); // OP
    }
  }

  @Override
  public int innerIterations() {
      return 3000;
  }
}

class VarHandleWeakCompareAndSetStaticFieldIntBenchmark extends MicroBenchmark {
  static final int FIELD_VALUE = 42;
  static int field = ~FIELD_VALUE;

  VarHandle vh;

  VarHandleWeakCompareAndSetStaticFieldIntBenchmark() throws Throwable {
    vh = MethodHandles.lookup().findStaticVarHandle(this.getClass(), "field", int.class);
  }

  @Override
  public void run() {
    boolean success;
    for (int pass = 0; pass < 100; ++pass) {
      success = vh.weakCompareAndSet(field, ~field); // OP
      success = vh.weakCompareAndSet(field, ~field); // OP
      success = vh.weakCompareAndSet(field, ~field); // OP
      success = vh.weakCompareAndSet(field, ~field); // OP
      success = vh.weakCompareAndSet(field, ~field); // OP
      success = vh.weakCompareAndSet(field, ~field); // OP
      success = vh.weakCompareAndSet(field, ~field); // OP
      success = vh.weakCompareAndSet(field, ~field); // OP
      success = vh.weakCompareAndSet(field, ~field); // OP
      success = vh.weakCompareAndSet(field, ~field); // OP
      success = vh.weakCompareAndSet(field, ~field); // OP
      success = vh.weakCompareAndSet(field, ~field); // OP
      success = vh.weakCompareAndSet(field, ~field); // OP
      success = vh.weakCompareAndSet(field, ~field); // OP
      success = vh.weakCompareAndSet(field, ~field); // OP
      success = vh.weakCompareAndSet(field, ~field); // OP
      success = vh.weakCompareAndSet(field, ~field); // OP
      success = vh.weakCompareAndSet(field, ~field); // OP
      success = vh.weakCompareAndSet(field, ~field); // OP
      success = vh.weakCompareAndSet(field, ~field); // OP
      success = vh.weakCompareAndSet(field, ~field); // OP
      success = vh.weakCompareAndSet(field, ~field); // OP
      success = vh.weakCompareAndSet(field, ~field); // OP
      success = vh.weakCompareAndSet(field, ~field); // OP
      success = vh.weakCompareAndSet(field, ~field); // OP
      success = vh.weakCompareAndSet(field, ~field); // OP
      success = vh.weakCompareAndSet(field, ~field); // OP
      success = vh.weakCompareAndSet(field, ~field); // OP
      success = vh.weakCompareAndSet(field, ~field); // OP
      success = vh.weakCompareAndSet(field, ~field); // OP
    }
  }

  @Override
  public int innerIterations() {
      return 3000;
  }
}

class VarHandleWeakCompareAndSetStaticFieldStringBenchmark extends MicroBenchmark {
  static final String FIELD_VALUE = "a string";
  static String field = null;

  VarHandle vh;

  VarHandleWeakCompareAndSetStaticFieldStringBenchmark() throws Throwable {
    vh = MethodHandles.lookup().findStaticVarHandle(this.getClass(), "field", String.class);
  }

  @Override
  public void run() {
    String newValue = (field != null) ? null : FIELD_VALUE;
    boolean success;
    for (int pass = 0; pass < 100; ++pass) {
      success = vh.weakCompareAndSet(field, newValue); // OP
      success = vh.weakCompareAndSet(field, newValue); // OP
      success = vh.weakCompareAndSet(field, newValue); // OP
      success = vh.weakCompareAndSet(field, newValue); // OP
      success = vh.weakCompareAndSet(field, newValue); // OP
      success = vh.weakCompareAndSet(field, newValue); // OP
      success = vh.weakCompareAndSet(field, newValue); // OP
      success = vh.weakCompareAndSet(field, newValue); // OP
      success = vh.weakCompareAndSet(field, newValue); // OP
      success = vh.weakCompareAndSet(field, newValue); // OP
      success = vh.weakCompareAndSet(field, newValue); // OP
      success = vh.weakCompareAndSet(field, newValue); // OP
      success = vh.weakCompareAndSet(field, newValue); // OP
      success = vh.weakCompareAndSet(field, newValue); // OP
      success = vh.weakCompareAndSet(field, newValue); // OP
      success = vh.weakCompareAndSet(field, newValue); // OP
      success = vh.weakCompareAndSet(field, newValue); // OP
      success = vh.weakCompareAndSet(field, newValue); // OP
      success = vh.weakCompareAndSet(field, newValue); // OP
      success = vh.weakCompareAndSet(field, newValue); // OP
      success = vh.weakCompareAndSet(field, newValue); // OP
      success = vh.weakCompareAndSet(field, newValue); // OP
      success = vh.weakCompareAndSet(field, newValue); // OP
      success = vh.weakCompareAndSet(field, newValue); // OP
      success = vh.weakCompareAndSet(field, newValue); // OP
      success = vh.weakCompareAndSet(field, newValue); // OP
      success = vh.weakCompareAndSet(field, newValue); // OP
      success = vh.weakCompareAndSet(field, newValue); // OP
      success = vh.weakCompareAndSet(field, newValue); // OP
      success = vh.weakCompareAndSet(field, newValue); // OP
    }
  }

  @Override
  public int innerIterations() {
      return 3000;
  }
}

class VarHandleWeakCompareAndSetFieldIntBenchmark extends MicroBenchmark {
  static final int FIELD_VALUE = 42;

  int field = FIELD_VALUE;
  VarHandle vh;

  VarHandleWeakCompareAndSetFieldIntBenchmark() throws Throwable {
    vh = MethodHandles.lookup().findVarHandle(this.getClass(), "field", int.class);
  }

  @Override
  public void run() {
    boolean success;
    for (int pass = 0; pass < 100; ++pass) {
      success = vh.weakCompareAndSet(this, field, ~field); // OP
      success = vh.weakCompareAndSet(this, field, ~field); // OP
      success = vh.weakCompareAndSet(this, field, ~field); // OP
      success = vh.weakCompareAndSet(this, field, ~field); // OP
      success = vh.weakCompareAndSet(this, field, ~field); // OP
      success = vh.weakCompareAndSet(this, field, ~field); // OP
      success = vh.weakCompareAndSet(this, field, ~field); // OP
      success = vh.weakCompareAndSet(this, field, ~field); // OP
      success = vh.weakCompareAndSet(this, field, ~field); // OP
      success = vh.weakCompareAndSet(this, field, ~field); // OP
      success = vh.weakCompareAndSet(this, field, ~field); // OP
      success = vh.weakCompareAndSet(this, field, ~field); // OP
      success = vh.weakCompareAndSet(this, field, ~field); // OP
      success = vh.weakCompareAndSet(this, field, ~field); // OP
      success = vh.weakCompareAndSet(this, field, ~field); // OP
      success = vh.weakCompareAndSet(this, field, ~field); // OP
      success = vh.weakCompareAndSet(this, field, ~field); // OP
      success = vh.weakCompareAndSet(this, field, ~field); // OP
      success = vh.weakCompareAndSet(this, field, ~field); // OP
      success = vh.weakCompareAndSet(this, field, ~field); // OP
      success = vh.weakCompareAndSet(this, field, ~field); // OP
      success = vh.weakCompareAndSet(this, field, ~field); // OP
      success = vh.weakCompareAndSet(this, field, ~field); // OP
      success = vh.weakCompareAndSet(this, field, ~field); // OP
      success = vh.weakCompareAndSet(this, field, ~field); // OP
      success = vh.weakCompareAndSet(this, field, ~field); // OP
      success = vh.weakCompareAndSet(this, field, ~field); // OP
      success = vh.weakCompareAndSet(this, field, ~field); // OP
      success = vh.weakCompareAndSet(this, field, ~field); // OP
      success = vh.weakCompareAndSet(this, field, ~field); // OP
    }
  }

  @Override
  public int innerIterations() {
      return 3000;
  }
}

class VarHandleWeakCompareAndSetFieldStringBenchmark extends MicroBenchmark {
  static final String FIELD_VALUE = "field value";
  String field = null;
  VarHandle vh;

  VarHandleWeakCompareAndSetFieldStringBenchmark() throws Throwable {
    vh = MethodHandles.lookup().findVarHandle(this.getClass(), "field", String.class);
  }

  @Override
  public void run() {
    String newValue = (field != null) ? null : FIELD_VALUE;
    boolean success;
    for (int pass = 0; pass < 100; ++pass) {
      success = vh.weakCompareAndSet(this, field, newValue); // OP
      success = vh.weakCompareAndSet(this, field, newValue); // OP
      success = vh.weakCompareAndSet(this, field, newValue); // OP
      success = vh.weakCompareAndSet(this, field, newValue); // OP
      success = vh.weakCompareAndSet(this, field, newValue); // OP
      success = vh.weakCompareAndSet(this, field, newValue); // OP
      success = vh.weakCompareAndSet(this, field, newValue); // OP
      success = vh.weakCompareAndSet(this, field, newValue); // OP
      success = vh.weakCompareAndSet(this, field, newValue); // OP
      success = vh.weakCompareAndSet(this, field, newValue); // OP
      success = vh.weakCompareAndSet(this, field, newValue); // OP
      success = vh.weakCompareAndSet(this, field, newValue); // OP
      success = vh.weakCompareAndSet(this, field, newValue); // OP
      success = vh.weakCompareAndSet(this, field, newValue); // OP
      success = vh.weakCompareAndSet(this, field, newValue); // OP
      success = vh.weakCompareAndSet(this, field, newValue); // OP
      success = vh.weakCompareAndSet(this, field, newValue); // OP
      success = vh.weakCompareAndSet(this, field, newValue); // OP
      success = vh.weakCompareAndSet(this, field, newValue); // OP
      success = vh.weakCompareAndSet(this, field, newValue); // OP
      success = vh.weakCompareAndSet(this, field, newValue); // OP
      success = vh.weakCompareAndSet(this, field, newValue); // OP
      success = vh.weakCompareAndSet(this, field, newValue); // OP
      success = vh.weakCompareAndSet(this, field, newValue); // OP
      success = vh.weakCompareAndSet(this, field, newValue); // OP
      success = vh.weakCompareAndSet(this, field, newValue); // OP
      success = vh.weakCompareAndSet(this, field, newValue); // OP
      success = vh.weakCompareAndSet(this, field, newValue); // OP
      success = vh.weakCompareAndSet(this, field, newValue); // OP
      success = vh.weakCompareAndSet(this, field, newValue); // OP
    }
  }

  @Override
  public int innerIterations() {
      return 3000;
  }
}

class VarHandleWeakCompareAndSetPlainStaticFieldIntBenchmark extends MicroBenchmark {
  static final int FIELD_VALUE = 42;
  static int field = ~FIELD_VALUE;

  VarHandle vh;

  VarHandleWeakCompareAndSetPlainStaticFieldIntBenchmark() throws Throwable {
    vh = MethodHandles.lookup().findStaticVarHandle(this.getClass(), "field", int.class);
  }

  @Override
  public void run() {
    boolean success;
    for (int pass = 0; pass < 100; ++pass) {
      success = vh.weakCompareAndSetPlain(field, ~field); // OP
      success = vh.weakCompareAndSetPlain(field, ~field); // OP
      success = vh.weakCompareAndSetPlain(field, ~field); // OP
      success = vh.weakCompareAndSetPlain(field, ~field); // OP
      success = vh.weakCompareAndSetPlain(field, ~field); // OP
      success = vh.weakCompareAndSetPlain(field, ~field); // OP
      success = vh.weakCompareAndSetPlain(field, ~field); // OP
      success = vh.weakCompareAndSetPlain(field, ~field); // OP
      success = vh.weakCompareAndSetPlain(field, ~field); // OP
      success = vh.weakCompareAndSetPlain(field, ~field); // OP
      success = vh.weakCompareAndSetPlain(field, ~field); // OP
      success = vh.weakCompareAndSetPlain(field, ~field); // OP
      success = vh.weakCompareAndSetPlain(field, ~field); // OP
      success = vh.weakCompareAndSetPlain(field, ~field); // OP
      success = vh.weakCompareAndSetPlain(field, ~field); // OP
      success = vh.weakCompareAndSetPlain(field, ~field); // OP
      success = vh.weakCompareAndSetPlain(field, ~field); // OP
      success = vh.weakCompareAndSetPlain(field, ~field); // OP
      success = vh.weakCompareAndSetPlain(field, ~field); // OP
      success = vh.weakCompareAndSetPlain(field, ~field); // OP
      success = vh.weakCompareAndSetPlain(field, ~field); // OP
      success = vh.weakCompareAndSetPlain(field, ~field); // OP
      success = vh.weakCompareAndSetPlain(field, ~field); // OP
      success = vh.weakCompareAndSetPlain(field, ~field); // OP
      success = vh.weakCompareAndSetPlain(field, ~field); // OP
      success = vh.weakCompareAndSetPlain(field, ~field); // OP
      success = vh.weakCompareAndSetPlain(field, ~field); // OP
      success = vh.weakCompareAndSetPlain(field, ~field); // OP
      success = vh.weakCompareAndSetPlain(field, ~field); // OP
      success = vh.weakCompareAndSetPlain(field, ~field); // OP
    }
  }

  @Override
  public int innerIterations() {
      return 3000;
  }
}

class VarHandleWeakCompareAndSetPlainStaticFieldStringBenchmark extends MicroBenchmark {
  static final String FIELD_VALUE = "a string";
  static String field = null;

  VarHandle vh;

  VarHandleWeakCompareAndSetPlainStaticFieldStringBenchmark() throws Throwable {
    vh = MethodHandles.lookup().findStaticVarHandle(this.getClass(), "field", String.class);
  }

  @Override
  public void run() {
    String newValue = (field != null) ? null : FIELD_VALUE;
    boolean success;
    for (int pass = 0; pass < 100; ++pass) {
      success = vh.weakCompareAndSetPlain(field, newValue); // OP
      success = vh.weakCompareAndSetPlain(field, newValue); // OP
      success = vh.weakCompareAndSetPlain(field, newValue); // OP
      success = vh.weakCompareAndSetPlain(field, newValue); // OP
      success = vh.weakCompareAndSetPlain(field, newValue); // OP
      success = vh.weakCompareAndSetPlain(field, newValue); // OP
      success = vh.weakCompareAndSetPlain(field, newValue); // OP
      success = vh.weakCompareAndSetPlain(field, newValue); // OP
      success = vh.weakCompareAndSetPlain(field, newValue); // OP
      success = vh.weakCompareAndSetPlain(field, newValue); // OP
      success = vh.weakCompareAndSetPlain(field, newValue); // OP
      success = vh.weakCompareAndSetPlain(field, newValue); // OP
      success = vh.weakCompareAndSetPlain(field, newValue); // OP
      success = vh.weakCompareAndSetPlain(field, newValue); // OP
      success = vh.weakCompareAndSetPlain(field, newValue); // OP
      success = vh.weakCompareAndSetPlain(field, newValue); // OP
      success = vh.weakCompareAndSetPlain(field, newValue); // OP
      success = vh.weakCompareAndSetPlain(field, newValue); // OP
      success = vh.weakCompareAndSetPlain(field, newValue); // OP
      success = vh.weakCompareAndSetPlain(field, newValue); // OP
      success = vh.weakCompareAndSetPlain(field, newValue); // OP
      success = vh.weakCompareAndSetPlain(field, newValue); // OP
      success = vh.weakCompareAndSetPlain(field, newValue); // OP
      success = vh.weakCompareAndSetPlain(field, newValue); // OP
      success = vh.weakCompareAndSetPlain(field, newValue); // OP
      success = vh.weakCompareAndSetPlain(field, newValue); // OP
      success = vh.weakCompareAndSetPlain(field, newValue); // OP
      success = vh.weakCompareAndSetPlain(field, newValue); // OP
      success = vh.weakCompareAndSetPlain(field, newValue); // OP
      success = vh.weakCompareAndSetPlain(field, newValue); // OP
    }
  }

  @Override
  public int innerIterations() {
      return 3000;
  }
}

class VarHandleWeakCompareAndSetPlainFieldIntBenchmark extends MicroBenchmark {
  static final int FIELD_VALUE = 42;

  int field = FIELD_VALUE;
  VarHandle vh;

  VarHandleWeakCompareAndSetPlainFieldIntBenchmark() throws Throwable {
    vh = MethodHandles.lookup().findVarHandle(this.getClass(), "field", int.class);
  }

  @Override
  public void run() {
    boolean success;
    for (int pass = 0; pass < 100; ++pass) {
      success = vh.weakCompareAndSetPlain(this, field, ~field); // OP
      success = vh.weakCompareAndSetPlain(this, field, ~field); // OP
      success = vh.weakCompareAndSetPlain(this, field, ~field); // OP
      success = vh.weakCompareAndSetPlain(this, field, ~field); // OP
      success = vh.weakCompareAndSetPlain(this, field, ~field); // OP
      success = vh.weakCompareAndSetPlain(this, field, ~field); // OP
      success = vh.weakCompareAndSetPlain(this, field, ~field); // OP
      success = vh.weakCompareAndSetPlain(this, field, ~field); // OP
      success = vh.weakCompareAndSetPlain(this, field, ~field); // OP
      success = vh.weakCompareAndSetPlain(this, field, ~field); // OP
      success = vh.weakCompareAndSetPlain(this, field, ~field); // OP
      success = vh.weakCompareAndSetPlain(this, field, ~field); // OP
      success = vh.weakCompareAndSetPlain(this, field, ~field); // OP
      success = vh.weakCompareAndSetPlain(this, field, ~field); // OP
      success = vh.weakCompareAndSetPlain(this, field, ~field); // OP
      success = vh.weakCompareAndSetPlain(this, field, ~field); // OP
      success = vh.weakCompareAndSetPlain(this, field, ~field); // OP
      success = vh.weakCompareAndSetPlain(this, field, ~field); // OP
      success = vh.weakCompareAndSetPlain(this, field, ~field); // OP
      success = vh.weakCompareAndSetPlain(this, field, ~field); // OP
      success = vh.weakCompareAndSetPlain(this, field, ~field); // OP
      success = vh.weakCompareAndSetPlain(this, field, ~field); // OP
      success = vh.weakCompareAndSetPlain(this, field, ~field); // OP
      success = vh.weakCompareAndSetPlain(this, field, ~field); // OP
      success = vh.weakCompareAndSetPlain(this, field, ~field); // OP
      success = vh.weakCompareAndSetPlain(this, field, ~field); // OP
      success = vh.weakCompareAndSetPlain(this, field, ~field); // OP
      success = vh.weakCompareAndSetPlain(this, field, ~field); // OP
      success = vh.weakCompareAndSetPlain(this, field, ~field); // OP
      success = vh.weakCompareAndSetPlain(this, field, ~field); // OP
    }
  }

  @Override
  public int innerIterations() {
      return 3000;
  }
}

class VarHandleWeakCompareAndSetPlainFieldStringBenchmark extends MicroBenchmark {
  static final String FIELD_VALUE = "field value";
  String field = null;
  VarHandle vh;

  VarHandleWeakCompareAndSetPlainFieldStringBenchmark() throws Throwable {
    vh = MethodHandles.lookup().findVarHandle(this.getClass(), "field", String.class);
  }

  @Override
  public void run() {
    String newValue = (field != null) ? null : FIELD_VALUE;
    boolean success;
    for (int pass = 0; pass < 100; ++pass) {
      success = vh.weakCompareAndSetPlain(this, field, newValue); // OP
      success = vh.weakCompareAndSetPlain(this, field, newValue); // OP
      success = vh.weakCompareAndSetPlain(this, field, newValue); // OP
      success = vh.weakCompareAndSetPlain(this, field, newValue); // OP
      success = vh.weakCompareAndSetPlain(this, field, newValue); // OP
      success = vh.weakCompareAndSetPlain(this, field, newValue); // OP
      success = vh.weakCompareAndSetPlain(this, field, newValue); // OP
      success = vh.weakCompareAndSetPlain(this, field, newValue); // OP
      success = vh.weakCompareAndSetPlain(this, field, newValue); // OP
      success = vh.weakCompareAndSetPlain(this, field, newValue); // OP
      success = vh.weakCompareAndSetPlain(this, field, newValue); // OP
      success = vh.weakCompareAndSetPlain(this, field, newValue); // OP
      success = vh.weakCompareAndSetPlain(this, field, newValue); // OP
      success = vh.weakCompareAndSetPlain(this, field, newValue); // OP
      success = vh.weakCompareAndSetPlain(this, field, newValue); // OP
      success = vh.weakCompareAndSetPlain(this, field, newValue); // OP
      success = vh.weakCompareAndSetPlain(this, field, newValue); // OP
      success = vh.weakCompareAndSetPlain(this, field, newValue); // OP
      success = vh.weakCompareAndSetPlain(this, field, newValue); // OP
      success = vh.weakCompareAndSetPlain(this, field, newValue); // OP
      success = vh.weakCompareAndSetPlain(this, field, newValue); // OP
      success = vh.weakCompareAndSetPlain(this, field, newValue); // OP
      success = vh.weakCompareAndSetPlain(this, field, newValue); // OP
      success = vh.weakCompareAndSetPlain(this, field, newValue); // OP
      success = vh.weakCompareAndSetPlain(this, field, newValue); // OP
      success = vh.weakCompareAndSetPlain(this, field, newValue); // OP
      success = vh.weakCompareAndSetPlain(this, field, newValue); // OP
      success = vh.weakCompareAndSetPlain(this, field, newValue); // OP
      success = vh.weakCompareAndSetPlain(this, field, newValue); // OP
      success = vh.weakCompareAndSetPlain(this, field, newValue); // OP
    }
  }

  @Override
  public int innerIterations() {
      return 3000;
  }
}

class VarHandleWeakCompareAndSetAcquireStaticFieldIntBenchmark extends MicroBenchmark {
  static final int FIELD_VALUE = 42;
  static int field = ~FIELD_VALUE;

  VarHandle vh;

  VarHandleWeakCompareAndSetAcquireStaticFieldIntBenchmark() throws Throwable {
    vh = MethodHandles.lookup().findStaticVarHandle(this.getClass(), "field", int.class);
  }

  @Override
  public void run() {
    boolean success;
    for (int pass = 0; pass < 100; ++pass) {
      success = vh.weakCompareAndSetAcquire(field, ~field); // OP
      success = vh.weakCompareAndSetAcquire(field, ~field); // OP
      success = vh.weakCompareAndSetAcquire(field, ~field); // OP
      success = vh.weakCompareAndSetAcquire(field, ~field); // OP
      success = vh.weakCompareAndSetAcquire(field, ~field); // OP
      success = vh.weakCompareAndSetAcquire(field, ~field); // OP
      success = vh.weakCompareAndSetAcquire(field, ~field); // OP
      success = vh.weakCompareAndSetAcquire(field, ~field); // OP
      success = vh.weakCompareAndSetAcquire(field, ~field); // OP
      success = vh.weakCompareAndSetAcquire(field, ~field); // OP
      success = vh.weakCompareAndSetAcquire(field, ~field); // OP
      success = vh.weakCompareAndSetAcquire(field, ~field); // OP
      success = vh.weakCompareAndSetAcquire(field, ~field); // OP
      success = vh.weakCompareAndSetAcquire(field, ~field); // OP
      success = vh.weakCompareAndSetAcquire(field, ~field); // OP
      success = vh.weakCompareAndSetAcquire(field, ~field); // OP
      success = vh.weakCompareAndSetAcquire(field, ~field); // OP
      success = vh.weakCompareAndSetAcquire(field, ~field); // OP
      success = vh.weakCompareAndSetAcquire(field, ~field); // OP
      success = vh.weakCompareAndSetAcquire(field, ~field); // OP
      success = vh.weakCompareAndSetAcquire(field, ~field); // OP
      success = vh.weakCompareAndSetAcquire(field, ~field); // OP
      success = vh.weakCompareAndSetAcquire(field, ~field); // OP
      success = vh.weakCompareAndSetAcquire(field, ~field); // OP
      success = vh.weakCompareAndSetAcquire(field, ~field); // OP
      success = vh.weakCompareAndSetAcquire(field, ~field); // OP
      success = vh.weakCompareAndSetAcquire(field, ~field); // OP
      success = vh.weakCompareAndSetAcquire(field, ~field); // OP
      success = vh.weakCompareAndSetAcquire(field, ~field); // OP
      success = vh.weakCompareAndSetAcquire(field, ~field); // OP
    }
  }

  @Override
  public int innerIterations() {
      return 3000;
  }
}

class VarHandleWeakCompareAndSetAcquireStaticFieldStringBenchmark extends MicroBenchmark {
  static final String FIELD_VALUE = "a string";
  static String field = null;

  VarHandle vh;

  VarHandleWeakCompareAndSetAcquireStaticFieldStringBenchmark() throws Throwable {
    vh = MethodHandles.lookup().findStaticVarHandle(this.getClass(), "field", String.class);
  }

  @Override
  public void run() {
    String newValue = (field != null) ? null : FIELD_VALUE;
    boolean success;
    for (int pass = 0; pass < 100; ++pass) {
      success = vh.weakCompareAndSetAcquire(field, newValue); // OP
      success = vh.weakCompareAndSetAcquire(field, newValue); // OP
      success = vh.weakCompareAndSetAcquire(field, newValue); // OP
      success = vh.weakCompareAndSetAcquire(field, newValue); // OP
      success = vh.weakCompareAndSetAcquire(field, newValue); // OP
      success = vh.weakCompareAndSetAcquire(field, newValue); // OP
      success = vh.weakCompareAndSetAcquire(field, newValue); // OP
      success = vh.weakCompareAndSetAcquire(field, newValue); // OP
      success = vh.weakCompareAndSetAcquire(field, newValue); // OP
      success = vh.weakCompareAndSetAcquire(field, newValue); // OP
      success = vh.weakCompareAndSetAcquire(field, newValue); // OP
      success = vh.weakCompareAndSetAcquire(field, newValue); // OP
      success = vh.weakCompareAndSetAcquire(field, newValue); // OP
      success = vh.weakCompareAndSetAcquire(field, newValue); // OP
      success = vh.weakCompareAndSetAcquire(field, newValue); // OP
      success = vh.weakCompareAndSetAcquire(field, newValue); // OP
      success = vh.weakCompareAndSetAcquire(field, newValue); // OP
      success = vh.weakCompareAndSetAcquire(field, newValue); // OP
      success = vh.weakCompareAndSetAcquire(field, newValue); // OP
      success = vh.weakCompareAndSetAcquire(field, newValue); // OP
      success = vh.weakCompareAndSetAcquire(field, newValue); // OP
      success = vh.weakCompareAndSetAcquire(field, newValue); // OP
      success = vh.weakCompareAndSetAcquire(field, newValue); // OP
      success = vh.weakCompareAndSetAcquire(field, newValue); // OP
      success = vh.weakCompareAndSetAcquire(field, newValue); // OP
      success = vh.weakCompareAndSetAcquire(field, newValue); // OP
      success = vh.weakCompareAndSetAcquire(field, newValue); // OP
      success = vh.weakCompareAndSetAcquire(field, newValue); // OP
      success = vh.weakCompareAndSetAcquire(field, newValue); // OP
      success = vh.weakCompareAndSetAcquire(field, newValue); // OP
    }
  }

  @Override
  public int innerIterations() {
      return 3000;
  }
}

class VarHandleWeakCompareAndSetAcquireFieldIntBenchmark extends MicroBenchmark {
  static final int FIELD_VALUE = 42;

  int field = FIELD_VALUE;
  VarHandle vh;

  VarHandleWeakCompareAndSetAcquireFieldIntBenchmark() throws Throwable {
    vh = MethodHandles.lookup().findVarHandle(this.getClass(), "field", int.class);
  }

  @Override
  public void run() {
    boolean success;
    for (int pass = 0; pass < 100; ++pass) {
      success = vh.weakCompareAndSetAcquire(this, field, ~field); // OP
      success = vh.weakCompareAndSetAcquire(this, field, ~field); // OP
      success = vh.weakCompareAndSetAcquire(this, field, ~field); // OP
      success = vh.weakCompareAndSetAcquire(this, field, ~field); // OP
      success = vh.weakCompareAndSetAcquire(this, field, ~field); // OP
      success = vh.weakCompareAndSetAcquire(this, field, ~field); // OP
      success = vh.weakCompareAndSetAcquire(this, field, ~field); // OP
      success = vh.weakCompareAndSetAcquire(this, field, ~field); // OP
      success = vh.weakCompareAndSetAcquire(this, field, ~field); // OP
      success = vh.weakCompareAndSetAcquire(this, field, ~field); // OP
      success = vh.weakCompareAndSetAcquire(this, field, ~field); // OP
      success = vh.weakCompareAndSetAcquire(this, field, ~field); // OP
      success = vh.weakCompareAndSetAcquire(this, field, ~field); // OP
      success = vh.weakCompareAndSetAcquire(this, field, ~field); // OP
      success = vh.weakCompareAndSetAcquire(this, field, ~field); // OP
      success = vh.weakCompareAndSetAcquire(this, field, ~field); // OP
      success = vh.weakCompareAndSetAcquire(this, field, ~field); // OP
      success = vh.weakCompareAndSetAcquire(this, field, ~field); // OP
      success = vh.weakCompareAndSetAcquire(this, field, ~field); // OP
      success = vh.weakCompareAndSetAcquire(this, field, ~field); // OP
      success = vh.weakCompareAndSetAcquire(this, field, ~field); // OP
      success = vh.weakCompareAndSetAcquire(this, field, ~field); // OP
      success = vh.weakCompareAndSetAcquire(this, field, ~field); // OP
      success = vh.weakCompareAndSetAcquire(this, field, ~field); // OP
      success = vh.weakCompareAndSetAcquire(this, field, ~field); // OP
      success = vh.weakCompareAndSetAcquire(this, field, ~field); // OP
      success = vh.weakCompareAndSetAcquire(this, field, ~field); // OP
      success = vh.weakCompareAndSetAcquire(this, field, ~field); // OP
      success = vh.weakCompareAndSetAcquire(this, field, ~field); // OP
      success = vh.weakCompareAndSetAcquire(this, field, ~field); // OP
    }
  }

  @Override
  public int innerIterations() {
      return 3000;
  }
}

class VarHandleWeakCompareAndSetAcquireFieldStringBenchmark extends MicroBenchmark {
  static final String FIELD_VALUE = "field value";
  String field = null;
  VarHandle vh;

  VarHandleWeakCompareAndSetAcquireFieldStringBenchmark() throws Throwable {
    vh = MethodHandles.lookup().findVarHandle(this.getClass(), "field", String.class);
  }

  @Override
  public void run() {
    String newValue = (field != null) ? null : FIELD_VALUE;
    boolean success;
    for (int pass = 0; pass < 100; ++pass) {
      success = vh.weakCompareAndSetAcquire(this, field, newValue); // OP
      success = vh.weakCompareAndSetAcquire(this, field, newValue); // OP
      success = vh.weakCompareAndSetAcquire(this, field, newValue); // OP
      success = vh.weakCompareAndSetAcquire(this, field, newValue); // OP
      success = vh.weakCompareAndSetAcquire(this, field, newValue); // OP
      success = vh.weakCompareAndSetAcquire(this, field, newValue); // OP
      success = vh.weakCompareAndSetAcquire(this, field, newValue); // OP
      success = vh.weakCompareAndSetAcquire(this, field, newValue); // OP
      success = vh.weakCompareAndSetAcquire(this, field, newValue); // OP
      success = vh.weakCompareAndSetAcquire(this, field, newValue); // OP
      success = vh.weakCompareAndSetAcquire(this, field, newValue); // OP
      success = vh.weakCompareAndSetAcquire(this, field, newValue); // OP
      success = vh.weakCompareAndSetAcquire(this, field, newValue); // OP
      success = vh.weakCompareAndSetAcquire(this, field, newValue); // OP
      success = vh.weakCompareAndSetAcquire(this, field, newValue); // OP
      success = vh.weakCompareAndSetAcquire(this, field, newValue); // OP
      success = vh.weakCompareAndSetAcquire(this, field, newValue); // OP
      success = vh.weakCompareAndSetAcquire(this, field, newValue); // OP
      success = vh.weakCompareAndSetAcquire(this, field, newValue); // OP
      success = vh.weakCompareAndSetAcquire(this, field, newValue); // OP
      success = vh.weakCompareAndSetAcquire(this, field, newValue); // OP
      success = vh.weakCompareAndSetAcquire(this, field, newValue); // OP
      success = vh.weakCompareAndSetAcquire(this, field, newValue); // OP
      success = vh.weakCompareAndSetAcquire(this, field, newValue); // OP
      success = vh.weakCompareAndSetAcquire(this, field, newValue); // OP
      success = vh.weakCompareAndSetAcquire(this, field, newValue); // OP
      success = vh.weakCompareAndSetAcquire(this, field, newValue); // OP
      success = vh.weakCompareAndSetAcquire(this, field, newValue); // OP
      success = vh.weakCompareAndSetAcquire(this, field, newValue); // OP
      success = vh.weakCompareAndSetAcquire(this, field, newValue); // OP
    }
  }

  @Override
  public int innerIterations() {
      return 3000;
  }
}

class VarHandleWeakCompareAndSetReleaseStaticFieldIntBenchmark extends MicroBenchmark {
  static final int FIELD_VALUE = 42;
  static int field = ~FIELD_VALUE;

  VarHandle vh;

  VarHandleWeakCompareAndSetReleaseStaticFieldIntBenchmark() throws Throwable {
    vh = MethodHandles.lookup().findStaticVarHandle(this.getClass(), "field", int.class);
  }

  @Override
  public void run() {
    boolean success;
    for (int pass = 0; pass < 100; ++pass) {
      success = vh.weakCompareAndSetRelease(field, ~field); // OP
      success = vh.weakCompareAndSetRelease(field, ~field); // OP
      success = vh.weakCompareAndSetRelease(field, ~field); // OP
      success = vh.weakCompareAndSetRelease(field, ~field); // OP
      success = vh.weakCompareAndSetRelease(field, ~field); // OP
      success = vh.weakCompareAndSetRelease(field, ~field); // OP
      success = vh.weakCompareAndSetRelease(field, ~field); // OP
      success = vh.weakCompareAndSetRelease(field, ~field); // OP
      success = vh.weakCompareAndSetRelease(field, ~field); // OP
      success = vh.weakCompareAndSetRelease(field, ~field); // OP
      success = vh.weakCompareAndSetRelease(field, ~field); // OP
      success = vh.weakCompareAndSetRelease(field, ~field); // OP
      success = vh.weakCompareAndSetRelease(field, ~field); // OP
      success = vh.weakCompareAndSetRelease(field, ~field); // OP
      success = vh.weakCompareAndSetRelease(field, ~field); // OP
      success = vh.weakCompareAndSetRelease(field, ~field); // OP
      success = vh.weakCompareAndSetRelease(field, ~field); // OP
      success = vh.weakCompareAndSetRelease(field, ~field); // OP
      success = vh.weakCompareAndSetRelease(field, ~field); // OP
      success = vh.weakCompareAndSetRelease(field, ~field); // OP
      success = vh.weakCompareAndSetRelease(field, ~field); // OP
      success = vh.weakCompareAndSetRelease(field, ~field); // OP
      success = vh.weakCompareAndSetRelease(field, ~field); // OP
      success = vh.weakCompareAndSetRelease(field, ~field); // OP
      success = vh.weakCompareAndSetRelease(field, ~field); // OP
      success = vh.weakCompareAndSetRelease(field, ~field); // OP
      success = vh.weakCompareAndSetRelease(field, ~field); // OP
      success = vh.weakCompareAndSetRelease(field, ~field); // OP
      success = vh.weakCompareAndSetRelease(field, ~field); // OP
      success = vh.weakCompareAndSetRelease(field, ~field); // OP
    }
  }

  @Override
  public int innerIterations() {
      return 3000;
  }
}

class VarHandleWeakCompareAndSetReleaseStaticFieldStringBenchmark extends MicroBenchmark {
  static final String FIELD_VALUE = "a string";
  static String field = null;

  VarHandle vh;

  VarHandleWeakCompareAndSetReleaseStaticFieldStringBenchmark() throws Throwable {
    vh = MethodHandles.lookup().findStaticVarHandle(this.getClass(), "field", String.class);
  }

  @Override
  public void run() {
    String newValue = (field != null) ? null : FIELD_VALUE;
    boolean success;
    for (int pass = 0; pass < 100; ++pass) {
      success = vh.weakCompareAndSetRelease(field, newValue); // OP
      success = vh.weakCompareAndSetRelease(field, newValue); // OP
      success = vh.weakCompareAndSetRelease(field, newValue); // OP
      success = vh.weakCompareAndSetRelease(field, newValue); // OP
      success = vh.weakCompareAndSetRelease(field, newValue); // OP
      success = vh.weakCompareAndSetRelease(field, newValue); // OP
      success = vh.weakCompareAndSetRelease(field, newValue); // OP
      success = vh.weakCompareAndSetRelease(field, newValue); // OP
      success = vh.weakCompareAndSetRelease(field, newValue); // OP
      success = vh.weakCompareAndSetRelease(field, newValue); // OP
      success = vh.weakCompareAndSetRelease(field, newValue); // OP
      success = vh.weakCompareAndSetRelease(field, newValue); // OP
      success = vh.weakCompareAndSetRelease(field, newValue); // OP
      success = vh.weakCompareAndSetRelease(field, newValue); // OP
      success = vh.weakCompareAndSetRelease(field, newValue); // OP
      success = vh.weakCompareAndSetRelease(field, newValue); // OP
      success = vh.weakCompareAndSetRelease(field, newValue); // OP
      success = vh.weakCompareAndSetRelease(field, newValue); // OP
      success = vh.weakCompareAndSetRelease(field, newValue); // OP
      success = vh.weakCompareAndSetRelease(field, newValue); // OP
      success = vh.weakCompareAndSetRelease(field, newValue); // OP
      success = vh.weakCompareAndSetRelease(field, newValue); // OP
      success = vh.weakCompareAndSetRelease(field, newValue); // OP
      success = vh.weakCompareAndSetRelease(field, newValue); // OP
      success = vh.weakCompareAndSetRelease(field, newValue); // OP
      success = vh.weakCompareAndSetRelease(field, newValue); // OP
      success = vh.weakCompareAndSetRelease(field, newValue); // OP
      success = vh.weakCompareAndSetRelease(field, newValue); // OP
      success = vh.weakCompareAndSetRelease(field, newValue); // OP
      success = vh.weakCompareAndSetRelease(field, newValue); // OP
    }
  }

  @Override
  public int innerIterations() {
      return 3000;
  }
}

class VarHandleWeakCompareAndSetReleaseFieldIntBenchmark extends MicroBenchmark {
  static final int FIELD_VALUE = 42;

  int field = FIELD_VALUE;
  VarHandle vh;

  VarHandleWeakCompareAndSetReleaseFieldIntBenchmark() throws Throwable {
    vh = MethodHandles.lookup().findVarHandle(this.getClass(), "field", int.class);
  }

  @Override
  public void run() {
    boolean success;
    for (int pass = 0; pass < 100; ++pass) {
      success = vh.weakCompareAndSetRelease(this, field, ~field); // OP
      success = vh.weakCompareAndSetRelease(this, field, ~field); // OP
      success = vh.weakCompareAndSetRelease(this, field, ~field); // OP
      success = vh.weakCompareAndSetRelease(this, field, ~field); // OP
      success = vh.weakCompareAndSetRelease(this, field, ~field); // OP
      success = vh.weakCompareAndSetRelease(this, field, ~field); // OP
      success = vh.weakCompareAndSetRelease(this, field, ~field); // OP
      success = vh.weakCompareAndSetRelease(this, field, ~field); // OP
      success = vh.weakCompareAndSetRelease(this, field, ~field); // OP
      success = vh.weakCompareAndSetRelease(this, field, ~field); // OP
      success = vh.weakCompareAndSetRelease(this, field, ~field); // OP
      success = vh.weakCompareAndSetRelease(this, field, ~field); // OP
      success = vh.weakCompareAndSetRelease(this, field, ~field); // OP
      success = vh.weakCompareAndSetRelease(this, field, ~field); // OP
      success = vh.weakCompareAndSetRelease(this, field, ~field); // OP
      success = vh.weakCompareAndSetRelease(this, field, ~field); // OP
      success = vh.weakCompareAndSetRelease(this, field, ~field); // OP
      success = vh.weakCompareAndSetRelease(this, field, ~field); // OP
      success = vh.weakCompareAndSetRelease(this, field, ~field); // OP
      success = vh.weakCompareAndSetRelease(this, field, ~field); // OP
      success = vh.weakCompareAndSetRelease(this, field, ~field); // OP
      success = vh.weakCompareAndSetRelease(this, field, ~field); // OP
      success = vh.weakCompareAndSetRelease(this, field, ~field); // OP
      success = vh.weakCompareAndSetRelease(this, field, ~field); // OP
      success = vh.weakCompareAndSetRelease(this, field, ~field); // OP
      success = vh.weakCompareAndSetRelease(this, field, ~field); // OP
      success = vh.weakCompareAndSetRelease(this, field, ~field); // OP
      success = vh.weakCompareAndSetRelease(this, field, ~field); // OP
      success = vh.weakCompareAndSetRelease(this, field, ~field); // OP
      success = vh.weakCompareAndSetRelease(this, field, ~field); // OP
    }
  }

  @Override
  public int innerIterations() {
      return 3000;
  }
}

class VarHandleWeakCompareAndSetReleaseFieldStringBenchmark extends MicroBenchmark {
  static final String FIELD_VALUE = "field value";
  String field = null;
  VarHandle vh;

  VarHandleWeakCompareAndSetReleaseFieldStringBenchmark() throws Throwable {
    vh = MethodHandles.lookup().findVarHandle(this.getClass(), "field", String.class);
  }

  @Override
  public void run() {
    String newValue = (field != null) ? null : FIELD_VALUE;
    boolean success;
    for (int pass = 0; pass < 100; ++pass) {
      success = vh.weakCompareAndSetRelease(this, field, newValue); // OP
      success = vh.weakCompareAndSetRelease(this, field, newValue); // OP
      success = vh.weakCompareAndSetRelease(this, field, newValue); // OP
      success = vh.weakCompareAndSetRelease(this, field, newValue); // OP
      success = vh.weakCompareAndSetRelease(this, field, newValue); // OP
      success = vh.weakCompareAndSetRelease(this, field, newValue); // OP
      success = vh.weakCompareAndSetRelease(this, field, newValue); // OP
      success = vh.weakCompareAndSetRelease(this, field, newValue); // OP
      success = vh.weakCompareAndSetRelease(this, field, newValue); // OP
      success = vh.weakCompareAndSetRelease(this, field, newValue); // OP
      success = vh.weakCompareAndSetRelease(this, field, newValue); // OP
      success = vh.weakCompareAndSetRelease(this, field, newValue); // OP
      success = vh.weakCompareAndSetRelease(this, field, newValue); // OP
      success = vh.weakCompareAndSetRelease(this, field, newValue); // OP
      success = vh.weakCompareAndSetRelease(this, field, newValue); // OP
      success = vh.weakCompareAndSetRelease(this, field, newValue); // OP
      success = vh.weakCompareAndSetRelease(this, field, newValue); // OP
      success = vh.weakCompareAndSetRelease(this, field, newValue); // OP
      success = vh.weakCompareAndSetRelease(this, field, newValue); // OP
      success = vh.weakCompareAndSetRelease(this, field, newValue); // OP
      success = vh.weakCompareAndSetRelease(this, field, newValue); // OP
      success = vh.weakCompareAndSetRelease(this, field, newValue); // OP
      success = vh.weakCompareAndSetRelease(this, field, newValue); // OP
      success = vh.weakCompareAndSetRelease(this, field, newValue); // OP
      success = vh.weakCompareAndSetRelease(this, field, newValue); // OP
      success = vh.weakCompareAndSetRelease(this, field, newValue); // OP
      success = vh.weakCompareAndSetRelease(this, field, newValue); // OP
      success = vh.weakCompareAndSetRelease(this, field, newValue); // OP
      success = vh.weakCompareAndSetRelease(this, field, newValue); // OP
      success = vh.weakCompareAndSetRelease(this, field, newValue); // OP
    }
  }

  @Override
  public int innerIterations() {
      return 3000;
  }
}

class VarHandleGetAndAddStaticFieldIntBenchmark extends MicroBenchmark {
  static final int FIELD_VALUE = 42;
  static final int ADD_VALUE = 10;

  static int field = FIELD_VALUE;
  VarHandle vh;

  VarHandleGetAndAddStaticFieldIntBenchmark() throws Throwable {
    vh = MethodHandles.lookup().findStaticVarHandle(this.getClass(), "field", int.class);
  }

  @Override
  public void run() {
    int x;
    for (int pass = 0; pass < 100; ++pass) {
      x = (int) vh.getAndAdd(ADD_VALUE); // OP
      x = (int) vh.getAndAdd(ADD_VALUE); // OP
      x = (int) vh.getAndAdd(ADD_VALUE); // OP
      x = (int) vh.getAndAdd(ADD_VALUE); // OP
      x = (int) vh.getAndAdd(ADD_VALUE); // OP
      x = (int) vh.getAndAdd(ADD_VALUE); // OP
      x = (int) vh.getAndAdd(ADD_VALUE); // OP
      x = (int) vh.getAndAdd(ADD_VALUE); // OP
      x = (int) vh.getAndAdd(ADD_VALUE); // OP
      x = (int) vh.getAndAdd(ADD_VALUE); // OP
      x = (int) vh.getAndAdd(ADD_VALUE); // OP
      x = (int) vh.getAndAdd(ADD_VALUE); // OP
      x = (int) vh.getAndAdd(ADD_VALUE); // OP
      x = (int) vh.getAndAdd(ADD_VALUE); // OP
      x = (int) vh.getAndAdd(ADD_VALUE); // OP
      x = (int) vh.getAndAdd(ADD_VALUE); // OP
      x = (int) vh.getAndAdd(ADD_VALUE); // OP
      x = (int) vh.getAndAdd(ADD_VALUE); // OP
      x = (int) vh.getAndAdd(ADD_VALUE); // OP
      x = (int) vh.getAndAdd(ADD_VALUE); // OP
      x = (int) vh.getAndAdd(ADD_VALUE); // OP
      x = (int) vh.getAndAdd(ADD_VALUE); // OP
      x = (int) vh.getAndAdd(ADD_VALUE); // OP
      x = (int) vh.getAndAdd(ADD_VALUE); // OP
      x = (int) vh.getAndAdd(ADD_VALUE); // OP
      x = (int) vh.getAndAdd(ADD_VALUE); // OP
      x = (int) vh.getAndAdd(ADD_VALUE); // OP
      x = (int) vh.getAndAdd(ADD_VALUE); // OP
      x = (int) vh.getAndAdd(ADD_VALUE); // OP
      x = (int) vh.getAndAdd(ADD_VALUE); // OP
    }
  }

  @Override
  public int innerIterations() {
      return 3000;
  }
}

class VarHandleGetAndAddFieldIntBenchmark extends MicroBenchmark {
  static final int FIELD_VALUE = 42;
  static final int ADD_VALUE = 10;

  int field = FIELD_VALUE;
  VarHandle vh;

  VarHandleGetAndAddFieldIntBenchmark() throws Throwable {
    vh = MethodHandles.lookup().findVarHandle(this.getClass(), "field", int.class);
  }

  @Override
  public void run() {
    int x;
    for (int pass = 0; pass < 100; ++pass) {
      x = (int) vh.getAndAdd(this, ADD_VALUE); // OP
      x = (int) vh.getAndAdd(this, ADD_VALUE); // OP
      x = (int) vh.getAndAdd(this, ADD_VALUE); // OP
      x = (int) vh.getAndAdd(this, ADD_VALUE); // OP
      x = (int) vh.getAndAdd(this, ADD_VALUE); // OP
      x = (int) vh.getAndAdd(this, ADD_VALUE); // OP
      x = (int) vh.getAndAdd(this, ADD_VALUE); // OP
      x = (int) vh.getAndAdd(this, ADD_VALUE); // OP
      x = (int) vh.getAndAdd(this, ADD_VALUE); // OP
      x = (int) vh.getAndAdd(this, ADD_VALUE); // OP
      x = (int) vh.getAndAdd(this, ADD_VALUE); // OP
      x = (int) vh.getAndAdd(this, ADD_VALUE); // OP
      x = (int) vh.getAndAdd(this, ADD_VALUE); // OP
      x = (int) vh.getAndAdd(this, ADD_VALUE); // OP
      x = (int) vh.getAndAdd(this, ADD_VALUE); // OP
      x = (int) vh.getAndAdd(this, ADD_VALUE); // OP
      x = (int) vh.getAndAdd(this, ADD_VALUE); // OP
      x = (int) vh.getAndAdd(this, ADD_VALUE); // OP
      x = (int) vh.getAndAdd(this, ADD_VALUE); // OP
      x = (int) vh.getAndAdd(this, ADD_VALUE); // OP
      x = (int) vh.getAndAdd(this, ADD_VALUE); // OP
      x = (int) vh.getAndAdd(this, ADD_VALUE); // OP
      x = (int) vh.getAndAdd(this, ADD_VALUE); // OP
      x = (int) vh.getAndAdd(this, ADD_VALUE); // OP
      x = (int) vh.getAndAdd(this, ADD_VALUE); // OP
      x = (int) vh.getAndAdd(this, ADD_VALUE); // OP
      x = (int) vh.getAndAdd(this, ADD_VALUE); // OP
      x = (int) vh.getAndAdd(this, ADD_VALUE); // OP
      x = (int) vh.getAndAdd(this, ADD_VALUE); // OP
      x = (int) vh.getAndAdd(this, ADD_VALUE); // OP
    }
  }

  @Override
  public int innerIterations() {
      return 3000;
  }
}

class VarHandleGetAndAddStaticFieldFloatBenchmark extends MicroBenchmark {
  static final float FIELD_VALUE = 42.4f;
  static final float ADD_VALUE = 10.11f;

  static float field = FIELD_VALUE;
  VarHandle vh;

  VarHandleGetAndAddStaticFieldFloatBenchmark() throws Throwable {
    vh = MethodHandles.lookup().findStaticVarHandle(this.getClass(), "field", float.class);
  }

  @Override
  public void run() {
    float x;
    for (int pass = 0; pass < 100; ++pass) {
      x = (float) vh.getAndAdd(ADD_VALUE); // OP
      x = (float) vh.getAndAdd(ADD_VALUE); // OP
      x = (float) vh.getAndAdd(ADD_VALUE); // OP
      x = (float) vh.getAndAdd(ADD_VALUE); // OP
      x = (float) vh.getAndAdd(ADD_VALUE); // OP
      x = (float) vh.getAndAdd(ADD_VALUE); // OP
      x = (float) vh.getAndAdd(ADD_VALUE); // OP
      x = (float) vh.getAndAdd(ADD_VALUE); // OP
      x = (float) vh.getAndAdd(ADD_VALUE); // OP
      x = (float) vh.getAndAdd(ADD_VALUE); // OP
      x = (float) vh.getAndAdd(ADD_VALUE); // OP
      x = (float) vh.getAndAdd(ADD_VALUE); // OP
      x = (float) vh.getAndAdd(ADD_VALUE); // OP
      x = (float) vh.getAndAdd(ADD_VALUE); // OP
      x = (float) vh.getAndAdd(ADD_VALUE); // OP
      x = (float) vh.getAndAdd(ADD_VALUE); // OP
      x = (float) vh.getAndAdd(ADD_VALUE); // OP
      x = (float) vh.getAndAdd(ADD_VALUE); // OP
      x = (float) vh.getAndAdd(ADD_VALUE); // OP
      x = (float) vh.getAndAdd(ADD_VALUE); // OP
      x = (float) vh.getAndAdd(ADD_VALUE); // OP
      x = (float) vh.getAndAdd(ADD_VALUE); // OP
      x = (float) vh.getAndAdd(ADD_VALUE); // OP
      x = (float) vh.getAndAdd(ADD_VALUE); // OP
      x = (float) vh.getAndAdd(ADD_VALUE); // OP
      x = (float) vh.getAndAdd(ADD_VALUE); // OP
      x = (float) vh.getAndAdd(ADD_VALUE); // OP
      x = (float) vh.getAndAdd(ADD_VALUE); // OP
      x = (float) vh.getAndAdd(ADD_VALUE); // OP
      x = (float) vh.getAndAdd(ADD_VALUE); // OP
    }
  }

  @Override
  public int innerIterations() {
      return 3000;
  }
}

class VarHandleGetAndAddFieldFloatBenchmark extends MicroBenchmark {
  static final float FIELD_VALUE = 42.4f;
  static final float ADD_VALUE = 10.11f;

  float field = FIELD_VALUE;
  VarHandle vh;

  VarHandleGetAndAddFieldFloatBenchmark() throws Throwable {
    vh = MethodHandles.lookup().findVarHandle(this.getClass(), "field", float.class);
  }

  @Override
  public void run() {
    float x;
    for (int pass = 0; pass < 100; ++pass) {
      x = (float) vh.getAndAdd(this, ADD_VALUE); // OP
      x = (float) vh.getAndAdd(this, ADD_VALUE); // OP
      x = (float) vh.getAndAdd(this, ADD_VALUE); // OP
      x = (float) vh.getAndAdd(this, ADD_VALUE); // OP
      x = (float) vh.getAndAdd(this, ADD_VALUE); // OP
      x = (float) vh.getAndAdd(this, ADD_VALUE); // OP
      x = (float) vh.getAndAdd(this, ADD_VALUE); // OP
      x = (float) vh.getAndAdd(this, ADD_VALUE); // OP
      x = (float) vh.getAndAdd(this, ADD_VALUE); // OP
      x = (float) vh.getAndAdd(this, ADD_VALUE); // OP
      x = (float) vh.getAndAdd(this, ADD_VALUE); // OP
      x = (float) vh.getAndAdd(this, ADD_VALUE); // OP
      x = (float) vh.getAndAdd(this, ADD_VALUE); // OP
      x = (float) vh.getAndAdd(this, ADD_VALUE); // OP
      x = (float) vh.getAndAdd(this, ADD_VALUE); // OP
      x = (float) vh.getAndAdd(this, ADD_VALUE); // OP
      x = (float) vh.getAndAdd(this, ADD_VALUE); // OP
      x = (float) vh.getAndAdd(this, ADD_VALUE); // OP
      x = (float) vh.getAndAdd(this, ADD_VALUE); // OP
      x = (float) vh.getAndAdd(this, ADD_VALUE); // OP
      x = (float) vh.getAndAdd(this, ADD_VALUE); // OP
      x = (float) vh.getAndAdd(this, ADD_VALUE); // OP
      x = (float) vh.getAndAdd(this, ADD_VALUE); // OP
      x = (float) vh.getAndAdd(this, ADD_VALUE); // OP
      x = (float) vh.getAndAdd(this, ADD_VALUE); // OP
      x = (float) vh.getAndAdd(this, ADD_VALUE); // OP
      x = (float) vh.getAndAdd(this, ADD_VALUE); // OP
      x = (float) vh.getAndAdd(this, ADD_VALUE); // OP
      x = (float) vh.getAndAdd(this, ADD_VALUE); // OP
      x = (float) vh.getAndAdd(this, ADD_VALUE); // OP
    }
  }

  @Override
  public int innerIterations() {
      return 3000;
  }
}

class VarHandleGetAndAddAcquireStaticFieldIntBenchmark extends MicroBenchmark {
  static final int FIELD_VALUE = 42;
  static final int ADD_VALUE = 10;

  static int field = FIELD_VALUE;
  VarHandle vh;

  VarHandleGetAndAddAcquireStaticFieldIntBenchmark() throws Throwable {
    vh = MethodHandles.lookup().findStaticVarHandle(this.getClass(), "field", int.class);
  }

  @Override
  public void run() {
    int x;
    for (int pass = 0; pass < 100; ++pass) {
      x = (int) vh.getAndAddAcquire(ADD_VALUE); // OP
      x = (int) vh.getAndAddAcquire(ADD_VALUE); // OP
      x = (int) vh.getAndAddAcquire(ADD_VALUE); // OP
      x = (int) vh.getAndAddAcquire(ADD_VALUE); // OP
      x = (int) vh.getAndAddAcquire(ADD_VALUE); // OP
      x = (int) vh.getAndAddAcquire(ADD_VALUE); // OP
      x = (int) vh.getAndAddAcquire(ADD_VALUE); // OP
      x = (int) vh.getAndAddAcquire(ADD_VALUE); // OP
      x = (int) vh.getAndAddAcquire(ADD_VALUE); // OP
      x = (int) vh.getAndAddAcquire(ADD_VALUE); // OP
      x = (int) vh.getAndAddAcquire(ADD_VALUE); // OP
      x = (int) vh.getAndAddAcquire(ADD_VALUE); // OP
      x = (int) vh.getAndAddAcquire(ADD_VALUE); // OP
      x = (int) vh.getAndAddAcquire(ADD_VALUE); // OP
      x = (int) vh.getAndAddAcquire(ADD_VALUE); // OP
      x = (int) vh.getAndAddAcquire(ADD_VALUE); // OP
      x = (int) vh.getAndAddAcquire(ADD_VALUE); // OP
      x = (int) vh.getAndAddAcquire(ADD_VALUE); // OP
      x = (int) vh.getAndAddAcquire(ADD_VALUE); // OP
      x = (int) vh.getAndAddAcquire(ADD_VALUE); // OP
      x = (int) vh.getAndAddAcquire(ADD_VALUE); // OP
      x = (int) vh.getAndAddAcquire(ADD_VALUE); // OP
      x = (int) vh.getAndAddAcquire(ADD_VALUE); // OP
      x = (int) vh.getAndAddAcquire(ADD_VALUE); // OP
      x = (int) vh.getAndAddAcquire(ADD_VALUE); // OP
      x = (int) vh.getAndAddAcquire(ADD_VALUE); // OP
      x = (int) vh.getAndAddAcquire(ADD_VALUE); // OP
      x = (int) vh.getAndAddAcquire(ADD_VALUE); // OP
      x = (int) vh.getAndAddAcquire(ADD_VALUE); // OP
      x = (int) vh.getAndAddAcquire(ADD_VALUE); // OP
    }
  }

  @Override
  public int innerIterations() {
      return 3000;
  }
}

class VarHandleGetAndAddAcquireFieldIntBenchmark extends MicroBenchmark {
  static final int FIELD_VALUE = 42;
  static final int ADD_VALUE = 10;

  int field = FIELD_VALUE;
  VarHandle vh;

  VarHandleGetAndAddAcquireFieldIntBenchmark() throws Throwable {
    vh = MethodHandles.lookup().findVarHandle(this.getClass(), "field", int.class);
  }

  @Override
  public void run() {
    int x;
    for (int pass = 0; pass < 100; ++pass) {
      x = (int) vh.getAndAddAcquire(this, ADD_VALUE); // OP
      x = (int) vh.getAndAddAcquire(this, ADD_VALUE); // OP
      x = (int) vh.getAndAddAcquire(this, ADD_VALUE); // OP
      x = (int) vh.getAndAddAcquire(this, ADD_VALUE); // OP
      x = (int) vh.getAndAddAcquire(this, ADD_VALUE); // OP
      x = (int) vh.getAndAddAcquire(this, ADD_VALUE); // OP
      x = (int) vh.getAndAddAcquire(this, ADD_VALUE); // OP
      x = (int) vh.getAndAddAcquire(this, ADD_VALUE); // OP
      x = (int) vh.getAndAddAcquire(this, ADD_VALUE); // OP
      x = (int) vh.getAndAddAcquire(this, ADD_VALUE); // OP
      x = (int) vh.getAndAddAcquire(this, ADD_VALUE); // OP
      x = (int) vh.getAndAddAcquire(this, ADD_VALUE); // OP
      x = (int) vh.getAndAddAcquire(this, ADD_VALUE); // OP
      x = (int) vh.getAndAddAcquire(this, ADD_VALUE); // OP
      x = (int) vh.getAndAddAcquire(this, ADD_VALUE); // OP
      x = (int) vh.getAndAddAcquire(this, ADD_VALUE); // OP
      x = (int) vh.getAndAddAcquire(this, ADD_VALUE); // OP
      x = (int) vh.getAndAddAcquire(this, ADD_VALUE); // OP
      x = (int) vh.getAndAddAcquire(this, ADD_VALUE); // OP
      x = (int) vh.getAndAddAcquire(this, ADD_VALUE); // OP
      x = (int) vh.getAndAddAcquire(this, ADD_VALUE); // OP
      x = (int) vh.getAndAddAcquire(this, ADD_VALUE); // OP
      x = (int) vh.getAndAddAcquire(this, ADD_VALUE); // OP
      x = (int) vh.getAndAddAcquire(this, ADD_VALUE); // OP
      x = (int) vh.getAndAddAcquire(this, ADD_VALUE); // OP
      x = (int) vh.getAndAddAcquire(this, ADD_VALUE); // OP
      x = (int) vh.getAndAddAcquire(this, ADD_VALUE); // OP
      x = (int) vh.getAndAddAcquire(this, ADD_VALUE); // OP
      x = (int) vh.getAndAddAcquire(this, ADD_VALUE); // OP
      x = (int) vh.getAndAddAcquire(this, ADD_VALUE); // OP
    }
  }

  @Override
  public int innerIterations() {
      return 3000;
  }
}

class VarHandleGetAndAddAcquireStaticFieldFloatBenchmark extends MicroBenchmark {
  static final float FIELD_VALUE = 42.4f;
  static final float ADD_VALUE = 10.11f;

  static float field = FIELD_VALUE;
  VarHandle vh;

  VarHandleGetAndAddAcquireStaticFieldFloatBenchmark() throws Throwable {
    vh = MethodHandles.lookup().findStaticVarHandle(this.getClass(), "field", float.class);
  }

  @Override
  public void run() {
    float x;
    for (int pass = 0; pass < 100; ++pass) {
      x = (float) vh.getAndAddAcquire(ADD_VALUE); // OP
      x = (float) vh.getAndAddAcquire(ADD_VALUE); // OP
      x = (float) vh.getAndAddAcquire(ADD_VALUE); // OP
      x = (float) vh.getAndAddAcquire(ADD_VALUE); // OP
      x = (float) vh.getAndAddAcquire(ADD_VALUE); // OP
      x = (float) vh.getAndAddAcquire(ADD_VALUE); // OP
      x = (float) vh.getAndAddAcquire(ADD_VALUE); // OP
      x = (float) vh.getAndAddAcquire(ADD_VALUE); // OP
      x = (float) vh.getAndAddAcquire(ADD_VALUE); // OP
      x = (float) vh.getAndAddAcquire(ADD_VALUE); // OP
      x = (float) vh.getAndAddAcquire(ADD_VALUE); // OP
      x = (float) vh.getAndAddAcquire(ADD_VALUE); // OP
      x = (float) vh.getAndAddAcquire(ADD_VALUE); // OP
      x = (float) vh.getAndAddAcquire(ADD_VALUE); // OP
      x = (float) vh.getAndAddAcquire(ADD_VALUE); // OP
      x = (float) vh.getAndAddAcquire(ADD_VALUE); // OP
      x = (float) vh.getAndAddAcquire(ADD_VALUE); // OP
      x = (float) vh.getAndAddAcquire(ADD_VALUE); // OP
      x = (float) vh.getAndAddAcquire(ADD_VALUE); // OP
      x = (float) vh.getAndAddAcquire(ADD_VALUE); // OP
      x = (float) vh.getAndAddAcquire(ADD_VALUE); // OP
      x = (float) vh.getAndAddAcquire(ADD_VALUE); // OP
      x = (float) vh.getAndAddAcquire(ADD_VALUE); // OP
      x = (float) vh.getAndAddAcquire(ADD_VALUE); // OP
      x = (float) vh.getAndAddAcquire(ADD_VALUE); // OP
      x = (float) vh.getAndAddAcquire(ADD_VALUE); // OP
      x = (float) vh.getAndAddAcquire(ADD_VALUE); // OP
      x = (float) vh.getAndAddAcquire(ADD_VALUE); // OP
      x = (float) vh.getAndAddAcquire(ADD_VALUE); // OP
      x = (float) vh.getAndAddAcquire(ADD_VALUE); // OP
    }
  }

  @Override
  public int innerIterations() {
      return 3000;
  }
}

class VarHandleGetAndAddAcquireFieldFloatBenchmark extends MicroBenchmark {
  static final float FIELD_VALUE = 42.4f;
  static final float ADD_VALUE = 10.11f;

  float field = FIELD_VALUE;
  VarHandle vh;

  VarHandleGetAndAddAcquireFieldFloatBenchmark() throws Throwable {
    vh = MethodHandles.lookup().findVarHandle(this.getClass(), "field", float.class);
  }

  @Override
  public void run() {
    float x;
    for (int pass = 0; pass < 100; ++pass) {
      x = (float) vh.getAndAddAcquire(this, ADD_VALUE); // OP
      x = (float) vh.getAndAddAcquire(this, ADD_VALUE); // OP
      x = (float) vh.getAndAddAcquire(this, ADD_VALUE); // OP
      x = (float) vh.getAndAddAcquire(this, ADD_VALUE); // OP
      x = (float) vh.getAndAddAcquire(this, ADD_VALUE); // OP
      x = (float) vh.getAndAddAcquire(this, ADD_VALUE); // OP
      x = (float) vh.getAndAddAcquire(this, ADD_VALUE); // OP
      x = (float) vh.getAndAddAcquire(this, ADD_VALUE); // OP
      x = (float) vh.getAndAddAcquire(this, ADD_VALUE); // OP
      x = (float) vh.getAndAddAcquire(this, ADD_VALUE); // OP
      x = (float) vh.getAndAddAcquire(this, ADD_VALUE); // OP
      x = (float) vh.getAndAddAcquire(this, ADD_VALUE); // OP
      x = (float) vh.getAndAddAcquire(this, ADD_VALUE); // OP
      x = (float) vh.getAndAddAcquire(this, ADD_VALUE); // OP
      x = (float) vh.getAndAddAcquire(this, ADD_VALUE); // OP
      x = (float) vh.getAndAddAcquire(this, ADD_VALUE); // OP
      x = (float) vh.getAndAddAcquire(this, ADD_VALUE); // OP
      x = (float) vh.getAndAddAcquire(this, ADD_VALUE); // OP
      x = (float) vh.getAndAddAcquire(this, ADD_VALUE); // OP
      x = (float) vh.getAndAddAcquire(this, ADD_VALUE); // OP
      x = (float) vh.getAndAddAcquire(this, ADD_VALUE); // OP
      x = (float) vh.getAndAddAcquire(this, ADD_VALUE); // OP
      x = (float) vh.getAndAddAcquire(this, ADD_VALUE); // OP
      x = (float) vh.getAndAddAcquire(this, ADD_VALUE); // OP
      x = (float) vh.getAndAddAcquire(this, ADD_VALUE); // OP
      x = (float) vh.getAndAddAcquire(this, ADD_VALUE); // OP
      x = (float) vh.getAndAddAcquire(this, ADD_VALUE); // OP
      x = (float) vh.getAndAddAcquire(this, ADD_VALUE); // OP
      x = (float) vh.getAndAddAcquire(this, ADD_VALUE); // OP
      x = (float) vh.getAndAddAcquire(this, ADD_VALUE); // OP
    }
  }

  @Override
  public int innerIterations() {
      return 3000;
  }
}

class VarHandleGetAndAddReleaseStaticFieldIntBenchmark extends MicroBenchmark {
  static final int FIELD_VALUE = 42;
  static final int ADD_VALUE = 10;

  static int field = FIELD_VALUE;
  VarHandle vh;

  VarHandleGetAndAddReleaseStaticFieldIntBenchmark() throws Throwable {
    vh = MethodHandles.lookup().findStaticVarHandle(this.getClass(), "field", int.class);
  }

  @Override
  public void run() {
    int x;
    for (int pass = 0; pass < 100; ++pass) {
      x = (int) vh.getAndAddRelease(ADD_VALUE); // OP
      x = (int) vh.getAndAddRelease(ADD_VALUE); // OP
      x = (int) vh.getAndAddRelease(ADD_VALUE); // OP
      x = (int) vh.getAndAddRelease(ADD_VALUE); // OP
      x = (int) vh.getAndAddRelease(ADD_VALUE); // OP
      x = (int) vh.getAndAddRelease(ADD_VALUE); // OP
      x = (int) vh.getAndAddRelease(ADD_VALUE); // OP
      x = (int) vh.getAndAddRelease(ADD_VALUE); // OP
      x = (int) vh.getAndAddRelease(ADD_VALUE); // OP
      x = (int) vh.getAndAddRelease(ADD_VALUE); // OP
      x = (int) vh.getAndAddRelease(ADD_VALUE); // OP
      x = (int) vh.getAndAddRelease(ADD_VALUE); // OP
      x = (int) vh.getAndAddRelease(ADD_VALUE); // OP
      x = (int) vh.getAndAddRelease(ADD_VALUE); // OP
      x = (int) vh.getAndAddRelease(ADD_VALUE); // OP
      x = (int) vh.getAndAddRelease(ADD_VALUE); // OP
      x = (int) vh.getAndAddRelease(ADD_VALUE); // OP
      x = (int) vh.getAndAddRelease(ADD_VALUE); // OP
      x = (int) vh.getAndAddRelease(ADD_VALUE); // OP
      x = (int) vh.getAndAddRelease(ADD_VALUE); // OP
      x = (int) vh.getAndAddRelease(ADD_VALUE); // OP
      x = (int) vh.getAndAddRelease(ADD_VALUE); // OP
      x = (int) vh.getAndAddRelease(ADD_VALUE); // OP
      x = (int) vh.getAndAddRelease(ADD_VALUE); // OP
      x = (int) vh.getAndAddRelease(ADD_VALUE); // OP
      x = (int) vh.getAndAddRelease(ADD_VALUE); // OP
      x = (int) vh.getAndAddRelease(ADD_VALUE); // OP
      x = (int) vh.getAndAddRelease(ADD_VALUE); // OP
      x = (int) vh.getAndAddRelease(ADD_VALUE); // OP
      x = (int) vh.getAndAddRelease(ADD_VALUE); // OP
    }
  }

  @Override
  public int innerIterations() {
      return 3000;
  }
}

class VarHandleGetAndAddReleaseFieldIntBenchmark extends MicroBenchmark {
  static final int FIELD_VALUE = 42;
  static final int ADD_VALUE = 10;

  int field = FIELD_VALUE;
  VarHandle vh;

  VarHandleGetAndAddReleaseFieldIntBenchmark() throws Throwable {
    vh = MethodHandles.lookup().findVarHandle(this.getClass(), "field", int.class);
  }

  @Override
  public void run() {
    int x;
    for (int pass = 0; pass < 100; ++pass) {
      x = (int) vh.getAndAddRelease(this, ADD_VALUE); // OP
      x = (int) vh.getAndAddRelease(this, ADD_VALUE); // OP
      x = (int) vh.getAndAddRelease(this, ADD_VALUE); // OP
      x = (int) vh.getAndAddRelease(this, ADD_VALUE); // OP
      x = (int) vh.getAndAddRelease(this, ADD_VALUE); // OP
      x = (int) vh.getAndAddRelease(this, ADD_VALUE); // OP
      x = (int) vh.getAndAddRelease(this, ADD_VALUE); // OP
      x = (int) vh.getAndAddRelease(this, ADD_VALUE); // OP
      x = (int) vh.getAndAddRelease(this, ADD_VALUE); // OP
      x = (int) vh.getAndAddRelease(this, ADD_VALUE); // OP
      x = (int) vh.getAndAddRelease(this, ADD_VALUE); // OP
      x = (int) vh.getAndAddRelease(this, ADD_VALUE); // OP
      x = (int) vh.getAndAddRelease(this, ADD_VALUE); // OP
      x = (int) vh.getAndAddRelease(this, ADD_VALUE); // OP
      x = (int) vh.getAndAddRelease(this, ADD_VALUE); // OP
      x = (int) vh.getAndAddRelease(this, ADD_VALUE); // OP
      x = (int) vh.getAndAddRelease(this, ADD_VALUE); // OP
      x = (int) vh.getAndAddRelease(this, ADD_VALUE); // OP
      x = (int) vh.getAndAddRelease(this, ADD_VALUE); // OP
      x = (int) vh.getAndAddRelease(this, ADD_VALUE); // OP
      x = (int) vh.getAndAddRelease(this, ADD_VALUE); // OP
      x = (int) vh.getAndAddRelease(this, ADD_VALUE); // OP
      x = (int) vh.getAndAddRelease(this, ADD_VALUE); // OP
      x = (int) vh.getAndAddRelease(this, ADD_VALUE); // OP
      x = (int) vh.getAndAddRelease(this, ADD_VALUE); // OP
      x = (int) vh.getAndAddRelease(this, ADD_VALUE); // OP
      x = (int) vh.getAndAddRelease(this, ADD_VALUE); // OP
      x = (int) vh.getAndAddRelease(this, ADD_VALUE); // OP
      x = (int) vh.getAndAddRelease(this, ADD_VALUE); // OP
      x = (int) vh.getAndAddRelease(this, ADD_VALUE); // OP
    }
  }

  @Override
  public int innerIterations() {
      return 3000;
  }
}

class VarHandleGetAndAddReleaseStaticFieldFloatBenchmark extends MicroBenchmark {
  static final float FIELD_VALUE = 42.4f;
  static final float ADD_VALUE = 10.11f;

  static float field = FIELD_VALUE;
  VarHandle vh;

  VarHandleGetAndAddReleaseStaticFieldFloatBenchmark() throws Throwable {
    vh = MethodHandles.lookup().findStaticVarHandle(this.getClass(), "field", float.class);
  }

  @Override
  public void run() {
    float x;
    for (int pass = 0; pass < 100; ++pass) {
      x = (float) vh.getAndAddRelease(ADD_VALUE); // OP
      x = (float) vh.getAndAddRelease(ADD_VALUE); // OP
      x = (float) vh.getAndAddRelease(ADD_VALUE); // OP
      x = (float) vh.getAndAddRelease(ADD_VALUE); // OP
      x = (float) vh.getAndAddRelease(ADD_VALUE); // OP
      x = (float) vh.getAndAddRelease(ADD_VALUE); // OP
      x = (float) vh.getAndAddRelease(ADD_VALUE); // OP
      x = (float) vh.getAndAddRelease(ADD_VALUE); // OP
      x = (float) vh.getAndAddRelease(ADD_VALUE); // OP
      x = (float) vh.getAndAddRelease(ADD_VALUE); // OP
      x = (float) vh.getAndAddRelease(ADD_VALUE); // OP
      x = (float) vh.getAndAddRelease(ADD_VALUE); // OP
      x = (float) vh.getAndAddRelease(ADD_VALUE); // OP
      x = (float) vh.getAndAddRelease(ADD_VALUE); // OP
      x = (float) vh.getAndAddRelease(ADD_VALUE); // OP
      x = (float) vh.getAndAddRelease(ADD_VALUE); // OP
      x = (float) vh.getAndAddRelease(ADD_VALUE); // OP
      x = (float) vh.getAndAddRelease(ADD_VALUE); // OP
      x = (float) vh.getAndAddRelease(ADD_VALUE); // OP
      x = (float) vh.getAndAddRelease(ADD_VALUE); // OP
      x = (float) vh.getAndAddRelease(ADD_VALUE); // OP
      x = (float) vh.getAndAddRelease(ADD_VALUE); // OP
      x = (float) vh.getAndAddRelease(ADD_VALUE); // OP
      x = (float) vh.getAndAddRelease(ADD_VALUE); // OP
      x = (float) vh.getAndAddRelease(ADD_VALUE); // OP
      x = (float) vh.getAndAddRelease(ADD_VALUE); // OP
      x = (float) vh.getAndAddRelease(ADD_VALUE); // OP
      x = (float) vh.getAndAddRelease(ADD_VALUE); // OP
      x = (float) vh.getAndAddRelease(ADD_VALUE); // OP
      x = (float) vh.getAndAddRelease(ADD_VALUE); // OP
    }
  }

  @Override
  public int innerIterations() {
      return 3000;
  }
}

class VarHandleGetAndAddReleaseFieldFloatBenchmark extends MicroBenchmark {
  static final float FIELD_VALUE = 42.4f;
  static final float ADD_VALUE = 10.11f;

  float field = FIELD_VALUE;
  VarHandle vh;

  VarHandleGetAndAddReleaseFieldFloatBenchmark() throws Throwable {
    vh = MethodHandles.lookup().findVarHandle(this.getClass(), "field", float.class);
  }

  @Override
  public void run() {
    float x;
    for (int pass = 0; pass < 100; ++pass) {
      x = (float) vh.getAndAddRelease(this, ADD_VALUE); // OP
      x = (float) vh.getAndAddRelease(this, ADD_VALUE); // OP
      x = (float) vh.getAndAddRelease(this, ADD_VALUE); // OP
      x = (float) vh.getAndAddRelease(this, ADD_VALUE); // OP
      x = (float) vh.getAndAddRelease(this, ADD_VALUE); // OP
      x = (float) vh.getAndAddRelease(this, ADD_VALUE); // OP
      x = (float) vh.getAndAddRelease(this, ADD_VALUE); // OP
      x = (float) vh.getAndAddRelease(this, ADD_VALUE); // OP
      x = (float) vh.getAndAddRelease(this, ADD_VALUE); // OP
      x = (float) vh.getAndAddRelease(this, ADD_VALUE); // OP
      x = (float) vh.getAndAddRelease(this, ADD_VALUE); // OP
      x = (float) vh.getAndAddRelease(this, ADD_VALUE); // OP
      x = (float) vh.getAndAddRelease(this, ADD_VALUE); // OP
      x = (float) vh.getAndAddRelease(this, ADD_VALUE); // OP
      x = (float) vh.getAndAddRelease(this, ADD_VALUE); // OP
      x = (float) vh.getAndAddRelease(this, ADD_VALUE); // OP
      x = (float) vh.getAndAddRelease(this, ADD_VALUE); // OP
      x = (float) vh.getAndAddRelease(this, ADD_VALUE); // OP
      x = (float) vh.getAndAddRelease(this, ADD_VALUE); // OP
      x = (float) vh.getAndAddRelease(this, ADD_VALUE); // OP
      x = (float) vh.getAndAddRelease(this, ADD_VALUE); // OP
      x = (float) vh.getAndAddRelease(this, ADD_VALUE); // OP
      x = (float) vh.getAndAddRelease(this, ADD_VALUE); // OP
      x = (float) vh.getAndAddRelease(this, ADD_VALUE); // OP
      x = (float) vh.getAndAddRelease(this, ADD_VALUE); // OP
      x = (float) vh.getAndAddRelease(this, ADD_VALUE); // OP
      x = (float) vh.getAndAddRelease(this, ADD_VALUE); // OP
      x = (float) vh.getAndAddRelease(this, ADD_VALUE); // OP
      x = (float) vh.getAndAddRelease(this, ADD_VALUE); // OP
      x = (float) vh.getAndAddRelease(this, ADD_VALUE); // OP
    }
  }

  @Override
  public int innerIterations() {
      return 3000;
  }
}

class VarHandleGetAndBitwiseOrStaticFieldIntBenchmark extends MicroBenchmark {
  static final int FIELD_VALUE = 42;
  static final int ADD_VALUE = 10;

  static int field = FIELD_VALUE;
  VarHandle vh;

  VarHandleGetAndBitwiseOrStaticFieldIntBenchmark() throws Throwable {
    vh = MethodHandles.lookup().findStaticVarHandle(this.getClass(), "field", int.class);
  }

  @Override
  public void run() {
    int x;
    for (int pass = 0; pass < 100; ++pass) {
      x = (int) vh.getAndBitwiseOr(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseOr(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseOr(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseOr(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseOr(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseOr(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseOr(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseOr(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseOr(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseOr(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseOr(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseOr(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseOr(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseOr(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseOr(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseOr(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseOr(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseOr(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseOr(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseOr(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseOr(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseOr(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseOr(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseOr(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseOr(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseOr(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseOr(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseOr(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseOr(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseOr(ADD_VALUE); // OP
    }
  }

  @Override
  public int innerIterations() {
      return 3000;
  }
}

class VarHandleGetAndBitwiseOrFieldIntBenchmark extends MicroBenchmark {
  static final int FIELD_VALUE = 42;
  static final int ADD_VALUE = 10;

  int field = FIELD_VALUE;
  VarHandle vh;

  VarHandleGetAndBitwiseOrFieldIntBenchmark() throws Throwable {
    vh = MethodHandles.lookup().findVarHandle(this.getClass(), "field", int.class);
  }

  @Override
  public void run() {
    int x;
    for (int pass = 0; pass < 100; ++pass) {
      x = (int) vh.getAndBitwiseOr(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseOr(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseOr(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseOr(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseOr(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseOr(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseOr(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseOr(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseOr(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseOr(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseOr(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseOr(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseOr(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseOr(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseOr(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseOr(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseOr(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseOr(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseOr(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseOr(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseOr(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseOr(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseOr(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseOr(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseOr(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseOr(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseOr(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseOr(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseOr(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseOr(this, ADD_VALUE); // OP
    }
  }

  @Override
  public int innerIterations() {
      return 3000;
  }
}

class VarHandleGetAndBitwiseOrAcquireStaticFieldIntBenchmark extends MicroBenchmark {
  static final int FIELD_VALUE = 42;
  static final int ADD_VALUE = 10;

  static int field = FIELD_VALUE;
  VarHandle vh;

  VarHandleGetAndBitwiseOrAcquireStaticFieldIntBenchmark() throws Throwable {
    vh = MethodHandles.lookup().findStaticVarHandle(this.getClass(), "field", int.class);
  }

  @Override
  public void run() {
    int x;
    for (int pass = 0; pass < 100; ++pass) {
      x = (int) vh.getAndBitwiseOrAcquire(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseOrAcquire(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseOrAcquire(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseOrAcquire(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseOrAcquire(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseOrAcquire(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseOrAcquire(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseOrAcquire(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseOrAcquire(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseOrAcquire(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseOrAcquire(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseOrAcquire(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseOrAcquire(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseOrAcquire(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseOrAcquire(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseOrAcquire(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseOrAcquire(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseOrAcquire(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseOrAcquire(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseOrAcquire(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseOrAcquire(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseOrAcquire(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseOrAcquire(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseOrAcquire(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseOrAcquire(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseOrAcquire(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseOrAcquire(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseOrAcquire(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseOrAcquire(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseOrAcquire(ADD_VALUE); // OP
    }
  }

  @Override
  public int innerIterations() {
      return 3000;
  }
}

class VarHandleGetAndBitwiseOrAcquireFieldIntBenchmark extends MicroBenchmark {
  static final int FIELD_VALUE = 42;
  static final int ADD_VALUE = 10;

  int field = FIELD_VALUE;
  VarHandle vh;

  VarHandleGetAndBitwiseOrAcquireFieldIntBenchmark() throws Throwable {
    vh = MethodHandles.lookup().findVarHandle(this.getClass(), "field", int.class);
  }

  @Override
  public void run() {
    int x;
    for (int pass = 0; pass < 100; ++pass) {
      x = (int) vh.getAndBitwiseOrAcquire(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseOrAcquire(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseOrAcquire(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseOrAcquire(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseOrAcquire(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseOrAcquire(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseOrAcquire(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseOrAcquire(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseOrAcquire(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseOrAcquire(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseOrAcquire(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseOrAcquire(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseOrAcquire(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseOrAcquire(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseOrAcquire(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseOrAcquire(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseOrAcquire(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseOrAcquire(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseOrAcquire(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseOrAcquire(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseOrAcquire(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseOrAcquire(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseOrAcquire(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseOrAcquire(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseOrAcquire(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseOrAcquire(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseOrAcquire(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseOrAcquire(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseOrAcquire(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseOrAcquire(this, ADD_VALUE); // OP
    }
  }

  @Override
  public int innerIterations() {
      return 3000;
  }
}

class VarHandleGetAndBitwiseOrReleaseStaticFieldIntBenchmark extends MicroBenchmark {
  static final int FIELD_VALUE = 42;
  static final int ADD_VALUE = 10;

  static int field = FIELD_VALUE;
  VarHandle vh;

  VarHandleGetAndBitwiseOrReleaseStaticFieldIntBenchmark() throws Throwable {
    vh = MethodHandles.lookup().findStaticVarHandle(this.getClass(), "field", int.class);
  }

  @Override
  public void run() {
    int x;
    for (int pass = 0; pass < 100; ++pass) {
      x = (int) vh.getAndBitwiseOrRelease(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseOrRelease(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseOrRelease(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseOrRelease(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseOrRelease(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseOrRelease(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseOrRelease(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseOrRelease(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseOrRelease(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseOrRelease(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseOrRelease(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseOrRelease(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseOrRelease(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseOrRelease(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseOrRelease(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseOrRelease(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseOrRelease(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseOrRelease(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseOrRelease(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseOrRelease(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseOrRelease(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseOrRelease(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseOrRelease(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseOrRelease(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseOrRelease(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseOrRelease(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseOrRelease(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseOrRelease(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseOrRelease(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseOrRelease(ADD_VALUE); // OP
    }
  }

  @Override
  public int innerIterations() {
      return 3000;
  }
}

class VarHandleGetAndBitwiseOrReleaseFieldIntBenchmark extends MicroBenchmark {
  static final int FIELD_VALUE = 42;
  static final int ADD_VALUE = 10;

  int field = FIELD_VALUE;
  VarHandle vh;

  VarHandleGetAndBitwiseOrReleaseFieldIntBenchmark() throws Throwable {
    vh = MethodHandles.lookup().findVarHandle(this.getClass(), "field", int.class);
  }

  @Override
  public void run() {
    int x;
    for (int pass = 0; pass < 100; ++pass) {
      x = (int) vh.getAndBitwiseOrRelease(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseOrRelease(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseOrRelease(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseOrRelease(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseOrRelease(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseOrRelease(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseOrRelease(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseOrRelease(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseOrRelease(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseOrRelease(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseOrRelease(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseOrRelease(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseOrRelease(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseOrRelease(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseOrRelease(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseOrRelease(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseOrRelease(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseOrRelease(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseOrRelease(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseOrRelease(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseOrRelease(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseOrRelease(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseOrRelease(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseOrRelease(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseOrRelease(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseOrRelease(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseOrRelease(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseOrRelease(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseOrRelease(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseOrRelease(this, ADD_VALUE); // OP
    }
  }

  @Override
  public int innerIterations() {
      return 3000;
  }
}

class VarHandleGetAndBitwiseXorStaticFieldIntBenchmark extends MicroBenchmark {
  static final int FIELD_VALUE = 42;
  static final int ADD_VALUE = 10;

  static int field = FIELD_VALUE;
  VarHandle vh;

  VarHandleGetAndBitwiseXorStaticFieldIntBenchmark() throws Throwable {
    vh = MethodHandles.lookup().findStaticVarHandle(this.getClass(), "field", int.class);
  }

  @Override
  public void run() {
    int x;
    for (int pass = 0; pass < 100; ++pass) {
      x = (int) vh.getAndBitwiseXor(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseXor(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseXor(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseXor(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseXor(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseXor(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseXor(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseXor(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseXor(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseXor(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseXor(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseXor(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseXor(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseXor(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseXor(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseXor(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseXor(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseXor(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseXor(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseXor(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseXor(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseXor(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseXor(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseXor(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseXor(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseXor(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseXor(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseXor(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseXor(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseXor(ADD_VALUE); // OP
    }
  }

  @Override
  public int innerIterations() {
      return 3000;
  }
}

class VarHandleGetAndBitwiseXorFieldIntBenchmark extends MicroBenchmark {
  static final int FIELD_VALUE = 42;
  static final int ADD_VALUE = 10;

  int field = FIELD_VALUE;
  VarHandle vh;

  VarHandleGetAndBitwiseXorFieldIntBenchmark() throws Throwable {
    vh = MethodHandles.lookup().findVarHandle(this.getClass(), "field", int.class);
  }

  @Override
  public void run() {
    int x;
    for (int pass = 0; pass < 100; ++pass) {
      x = (int) vh.getAndBitwiseXor(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseXor(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseXor(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseXor(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseXor(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseXor(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseXor(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseXor(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseXor(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseXor(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseXor(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseXor(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseXor(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseXor(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseXor(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseXor(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseXor(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseXor(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseXor(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseXor(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseXor(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseXor(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseXor(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseXor(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseXor(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseXor(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseXor(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseXor(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseXor(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseXor(this, ADD_VALUE); // OP
    }
  }

  @Override
  public int innerIterations() {
      return 3000;
  }
}

class VarHandleGetAndBitwiseXorAcquireStaticFieldIntBenchmark extends MicroBenchmark {
  static final int FIELD_VALUE = 42;
  static final int ADD_VALUE = 10;

  static int field = FIELD_VALUE;
  VarHandle vh;

  VarHandleGetAndBitwiseXorAcquireStaticFieldIntBenchmark() throws Throwable {
    vh = MethodHandles.lookup().findStaticVarHandle(this.getClass(), "field", int.class);
  }

  @Override
  public void run() {
    int x;
    for (int pass = 0; pass < 100; ++pass) {
      x = (int) vh.getAndBitwiseXorAcquire(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseXorAcquire(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseXorAcquire(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseXorAcquire(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseXorAcquire(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseXorAcquire(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseXorAcquire(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseXorAcquire(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseXorAcquire(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseXorAcquire(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseXorAcquire(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseXorAcquire(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseXorAcquire(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseXorAcquire(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseXorAcquire(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseXorAcquire(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseXorAcquire(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseXorAcquire(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseXorAcquire(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseXorAcquire(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseXorAcquire(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseXorAcquire(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseXorAcquire(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseXorAcquire(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseXorAcquire(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseXorAcquire(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseXorAcquire(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseXorAcquire(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseXorAcquire(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseXorAcquire(ADD_VALUE); // OP
    }
  }

  @Override
  public int innerIterations() {
      return 3000;
  }
}

class VarHandleGetAndBitwiseXorAcquireFieldIntBenchmark extends MicroBenchmark {
  static final int FIELD_VALUE = 42;
  static final int ADD_VALUE = 10;

  int field = FIELD_VALUE;
  VarHandle vh;

  VarHandleGetAndBitwiseXorAcquireFieldIntBenchmark() throws Throwable {
    vh = MethodHandles.lookup().findVarHandle(this.getClass(), "field", int.class);
  }

  @Override
  public void run() {
    int x;
    for (int pass = 0; pass < 100; ++pass) {
      x = (int) vh.getAndBitwiseXorAcquire(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseXorAcquire(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseXorAcquire(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseXorAcquire(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseXorAcquire(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseXorAcquire(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseXorAcquire(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseXorAcquire(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseXorAcquire(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseXorAcquire(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseXorAcquire(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseXorAcquire(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseXorAcquire(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseXorAcquire(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseXorAcquire(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseXorAcquire(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseXorAcquire(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseXorAcquire(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseXorAcquire(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseXorAcquire(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseXorAcquire(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseXorAcquire(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseXorAcquire(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseXorAcquire(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseXorAcquire(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseXorAcquire(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseXorAcquire(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseXorAcquire(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseXorAcquire(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseXorAcquire(this, ADD_VALUE); // OP
    }
  }

  @Override
  public int innerIterations() {
      return 3000;
  }
}

class VarHandleGetAndBitwiseXorReleaseStaticFieldIntBenchmark extends MicroBenchmark {
  static final int FIELD_VALUE = 42;
  static final int ADD_VALUE = 10;

  static int field = FIELD_VALUE;
  VarHandle vh;

  VarHandleGetAndBitwiseXorReleaseStaticFieldIntBenchmark() throws Throwable {
    vh = MethodHandles.lookup().findStaticVarHandle(this.getClass(), "field", int.class);
  }

  @Override
  public void run() {
    int x;
    for (int pass = 0; pass < 100; ++pass) {
      x = (int) vh.getAndBitwiseXorRelease(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseXorRelease(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseXorRelease(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseXorRelease(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseXorRelease(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseXorRelease(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseXorRelease(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseXorRelease(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseXorRelease(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseXorRelease(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseXorRelease(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseXorRelease(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseXorRelease(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseXorRelease(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseXorRelease(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseXorRelease(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseXorRelease(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseXorRelease(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseXorRelease(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseXorRelease(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseXorRelease(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseXorRelease(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseXorRelease(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseXorRelease(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseXorRelease(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseXorRelease(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseXorRelease(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseXorRelease(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseXorRelease(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseXorRelease(ADD_VALUE); // OP
    }
  }

  @Override
  public int innerIterations() {
      return 3000;
  }
}

class VarHandleGetAndBitwiseXorReleaseFieldIntBenchmark extends MicroBenchmark {
  static final int FIELD_VALUE = 42;
  static final int ADD_VALUE = 10;

  int field = FIELD_VALUE;
  VarHandle vh;

  VarHandleGetAndBitwiseXorReleaseFieldIntBenchmark() throws Throwable {
    vh = MethodHandles.lookup().findVarHandle(this.getClass(), "field", int.class);
  }

  @Override
  public void run() {
    int x;
    for (int pass = 0; pass < 100; ++pass) {
      x = (int) vh.getAndBitwiseXorRelease(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseXorRelease(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseXorRelease(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseXorRelease(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseXorRelease(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseXorRelease(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseXorRelease(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseXorRelease(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseXorRelease(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseXorRelease(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseXorRelease(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseXorRelease(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseXorRelease(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseXorRelease(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseXorRelease(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseXorRelease(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseXorRelease(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseXorRelease(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseXorRelease(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseXorRelease(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseXorRelease(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseXorRelease(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseXorRelease(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseXorRelease(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseXorRelease(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseXorRelease(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseXorRelease(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseXorRelease(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseXorRelease(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseXorRelease(this, ADD_VALUE); // OP
    }
  }

  @Override
  public int innerIterations() {
      return 3000;
  }
}

class VarHandleGetAndBitwiseAndStaticFieldIntBenchmark extends MicroBenchmark {
  static final int FIELD_VALUE = 42;
  static final int ADD_VALUE = 10;

  static int field = FIELD_VALUE;
  VarHandle vh;

  VarHandleGetAndBitwiseAndStaticFieldIntBenchmark() throws Throwable {
    vh = MethodHandles.lookup().findStaticVarHandle(this.getClass(), "field", int.class);
  }

  @Override
  public void run() {
    int x;
    for (int pass = 0; pass < 100; ++pass) {
      x = (int) vh.getAndBitwiseAnd(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseAnd(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseAnd(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseAnd(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseAnd(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseAnd(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseAnd(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseAnd(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseAnd(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseAnd(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseAnd(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseAnd(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseAnd(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseAnd(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseAnd(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseAnd(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseAnd(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseAnd(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseAnd(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseAnd(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseAnd(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseAnd(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseAnd(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseAnd(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseAnd(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseAnd(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseAnd(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseAnd(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseAnd(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseAnd(ADD_VALUE); // OP
    }
  }

  @Override
  public int innerIterations() {
      return 3000;
  }
}

class VarHandleGetAndBitwiseAndFieldIntBenchmark extends MicroBenchmark {
  static final int FIELD_VALUE = 42;
  static final int ADD_VALUE = 10;

  int field = FIELD_VALUE;
  VarHandle vh;

  VarHandleGetAndBitwiseAndFieldIntBenchmark() throws Throwable {
    vh = MethodHandles.lookup().findVarHandle(this.getClass(), "field", int.class);
  }

  @Override
  public void run() {
    int x;
    for (int pass = 0; pass < 100; ++pass) {
      x = (int) vh.getAndBitwiseAnd(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseAnd(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseAnd(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseAnd(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseAnd(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseAnd(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseAnd(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseAnd(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseAnd(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseAnd(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseAnd(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseAnd(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseAnd(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseAnd(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseAnd(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseAnd(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseAnd(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseAnd(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseAnd(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseAnd(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseAnd(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseAnd(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseAnd(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseAnd(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseAnd(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseAnd(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseAnd(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseAnd(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseAnd(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseAnd(this, ADD_VALUE); // OP
    }
  }

  @Override
  public int innerIterations() {
      return 3000;
  }
}

class VarHandleGetAndBitwiseAndAcquireStaticFieldIntBenchmark extends MicroBenchmark {
  static final int FIELD_VALUE = 42;
  static final int ADD_VALUE = 10;

  static int field = FIELD_VALUE;
  VarHandle vh;

  VarHandleGetAndBitwiseAndAcquireStaticFieldIntBenchmark() throws Throwable {
    vh = MethodHandles.lookup().findStaticVarHandle(this.getClass(), "field", int.class);
  }

  @Override
  public void run() {
    int x;
    for (int pass = 0; pass < 100; ++pass) {
      x = (int) vh.getAndBitwiseAndAcquire(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseAndAcquire(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseAndAcquire(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseAndAcquire(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseAndAcquire(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseAndAcquire(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseAndAcquire(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseAndAcquire(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseAndAcquire(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseAndAcquire(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseAndAcquire(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseAndAcquire(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseAndAcquire(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseAndAcquire(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseAndAcquire(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseAndAcquire(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseAndAcquire(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseAndAcquire(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseAndAcquire(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseAndAcquire(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseAndAcquire(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseAndAcquire(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseAndAcquire(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseAndAcquire(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseAndAcquire(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseAndAcquire(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseAndAcquire(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseAndAcquire(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseAndAcquire(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseAndAcquire(ADD_VALUE); // OP
    }
  }

  @Override
  public int innerIterations() {
      return 3000;
  }
}

class VarHandleGetAndBitwiseAndAcquireFieldIntBenchmark extends MicroBenchmark {
  static final int FIELD_VALUE = 42;
  static final int ADD_VALUE = 10;

  int field = FIELD_VALUE;
  VarHandle vh;

  VarHandleGetAndBitwiseAndAcquireFieldIntBenchmark() throws Throwable {
    vh = MethodHandles.lookup().findVarHandle(this.getClass(), "field", int.class);
  }

  @Override
  public void run() {
    int x;
    for (int pass = 0; pass < 100; ++pass) {
      x = (int) vh.getAndBitwiseAndAcquire(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseAndAcquire(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseAndAcquire(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseAndAcquire(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseAndAcquire(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseAndAcquire(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseAndAcquire(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseAndAcquire(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseAndAcquire(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseAndAcquire(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseAndAcquire(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseAndAcquire(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseAndAcquire(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseAndAcquire(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseAndAcquire(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseAndAcquire(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseAndAcquire(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseAndAcquire(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseAndAcquire(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseAndAcquire(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseAndAcquire(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseAndAcquire(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseAndAcquire(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseAndAcquire(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseAndAcquire(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseAndAcquire(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseAndAcquire(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseAndAcquire(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseAndAcquire(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseAndAcquire(this, ADD_VALUE); // OP
    }
  }

  @Override
  public int innerIterations() {
      return 3000;
  }
}

class VarHandleGetAndBitwiseAndReleaseStaticFieldIntBenchmark extends MicroBenchmark {
  static final int FIELD_VALUE = 42;
  static final int ADD_VALUE = 10;

  static int field = FIELD_VALUE;
  VarHandle vh;

  VarHandleGetAndBitwiseAndReleaseStaticFieldIntBenchmark() throws Throwable {
    vh = MethodHandles.lookup().findStaticVarHandle(this.getClass(), "field", int.class);
  }

  @Override
  public void run() {
    int x;
    for (int pass = 0; pass < 100; ++pass) {
      x = (int) vh.getAndBitwiseAndRelease(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseAndRelease(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseAndRelease(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseAndRelease(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseAndRelease(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseAndRelease(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseAndRelease(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseAndRelease(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseAndRelease(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseAndRelease(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseAndRelease(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseAndRelease(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseAndRelease(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseAndRelease(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseAndRelease(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseAndRelease(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseAndRelease(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseAndRelease(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseAndRelease(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseAndRelease(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseAndRelease(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseAndRelease(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseAndRelease(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseAndRelease(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseAndRelease(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseAndRelease(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseAndRelease(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseAndRelease(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseAndRelease(ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseAndRelease(ADD_VALUE); // OP
    }
  }

  @Override
  public int innerIterations() {
      return 3000;
  }
}

class VarHandleGetAndBitwiseAndReleaseFieldIntBenchmark extends MicroBenchmark {
  static final int FIELD_VALUE = 42;
  static final int ADD_VALUE = 10;

  int field = FIELD_VALUE;
  VarHandle vh;

  VarHandleGetAndBitwiseAndReleaseFieldIntBenchmark() throws Throwable {
    vh = MethodHandles.lookup().findVarHandle(this.getClass(), "field", int.class);
  }

  @Override
  public void run() {
    int x;
    for (int pass = 0; pass < 100; ++pass) {
      x = (int) vh.getAndBitwiseAndRelease(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseAndRelease(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseAndRelease(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseAndRelease(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseAndRelease(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseAndRelease(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseAndRelease(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseAndRelease(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseAndRelease(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseAndRelease(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseAndRelease(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseAndRelease(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseAndRelease(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseAndRelease(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseAndRelease(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseAndRelease(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseAndRelease(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseAndRelease(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseAndRelease(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseAndRelease(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseAndRelease(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseAndRelease(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseAndRelease(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseAndRelease(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseAndRelease(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseAndRelease(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseAndRelease(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseAndRelease(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseAndRelease(this, ADD_VALUE); // OP
      x = (int) vh.getAndBitwiseAndRelease(this, ADD_VALUE); // OP
    }
  }

  @Override
  public int innerIterations() {
      return 3000;
  }
}

class VarHandleGetAndSetStaticFieldIntBenchmark extends MicroBenchmark {
  static final int FIELD_VALUE = 42;
  static int field = ~FIELD_VALUE;

  VarHandle vh;

  VarHandleGetAndSetStaticFieldIntBenchmark() throws Throwable {
    vh = MethodHandles.lookup().findStaticVarHandle(this.getClass(), "field", int.class);
  }

  @Override
  public void teardown() {
    if (field != FIELD_VALUE) {
      throw new RuntimeException("field has unexpected value: " + field);
    }
  }

  @Override
  public void run() {
    int x;
    for (int pass = 0; pass < 100; ++pass) {
      x = (int) vh.getAndSet(FIELD_VALUE); // OP
      x = (int) vh.getAndSet(FIELD_VALUE); // OP
      x = (int) vh.getAndSet(FIELD_VALUE); // OP
      x = (int) vh.getAndSet(FIELD_VALUE); // OP
      x = (int) vh.getAndSet(FIELD_VALUE); // OP
      x = (int) vh.getAndSet(FIELD_VALUE); // OP
      x = (int) vh.getAndSet(FIELD_VALUE); // OP
      x = (int) vh.getAndSet(FIELD_VALUE); // OP
      x = (int) vh.getAndSet(FIELD_VALUE); // OP
      x = (int) vh.getAndSet(FIELD_VALUE); // OP
      x = (int) vh.getAndSet(FIELD_VALUE); // OP
      x = (int) vh.getAndSet(FIELD_VALUE); // OP
      x = (int) vh.getAndSet(FIELD_VALUE); // OP
      x = (int) vh.getAndSet(FIELD_VALUE); // OP
      x = (int) vh.getAndSet(FIELD_VALUE); // OP
      x = (int) vh.getAndSet(FIELD_VALUE); // OP
      x = (int) vh.getAndSet(FIELD_VALUE); // OP
      x = (int) vh.getAndSet(FIELD_VALUE); // OP
      x = (int) vh.getAndSet(FIELD_VALUE); // OP
      x = (int) vh.getAndSet(FIELD_VALUE); // OP
      x = (int) vh.getAndSet(FIELD_VALUE); // OP
      x = (int) vh.getAndSet(FIELD_VALUE); // OP
      x = (int) vh.getAndSet(FIELD_VALUE); // OP
      x = (int) vh.getAndSet(FIELD_VALUE); // OP
      x = (int) vh.getAndSet(FIELD_VALUE); // OP
      x = (int) vh.getAndSet(FIELD_VALUE); // OP
      x = (int) vh.getAndSet(FIELD_VALUE); // OP
      x = (int) vh.getAndSet(FIELD_VALUE); // OP
      x = (int) vh.getAndSet(FIELD_VALUE); // OP
      x = (int) vh.getAndSet(FIELD_VALUE); // OP
    }
  }

  @Override
  public int innerIterations() {
      return 3000;
  }
}

class VarHandleGetAndSetStaticFieldStringBenchmark extends MicroBenchmark {
  static final String FIELD_VALUE = "a string";
  static String field = null;

  VarHandle vh;

  VarHandleGetAndSetStaticFieldStringBenchmark() throws Throwable {
    vh = MethodHandles.lookup().findStaticVarHandle(this.getClass(), "field", String.class);
  }

  @Override
  public void run() {
    String x;
    for (int pass = 0; pass < 100; ++pass) {
      x = (String) vh.getAndSet(FIELD_VALUE); // OP
      x = (String) vh.getAndSet(FIELD_VALUE); // OP
      x = (String) vh.getAndSet(FIELD_VALUE); // OP
      x = (String) vh.getAndSet(FIELD_VALUE); // OP
      x = (String) vh.getAndSet(FIELD_VALUE); // OP
      x = (String) vh.getAndSet(FIELD_VALUE); // OP
      x = (String) vh.getAndSet(FIELD_VALUE); // OP
      x = (String) vh.getAndSet(FIELD_VALUE); // OP
      x = (String) vh.getAndSet(FIELD_VALUE); // OP
      x = (String) vh.getAndSet(FIELD_VALUE); // OP
      x = (String) vh.getAndSet(FIELD_VALUE); // OP
      x = (String) vh.getAndSet(FIELD_VALUE); // OP
      x = (String) vh.getAndSet(FIELD_VALUE); // OP
      x = (String) vh.getAndSet(FIELD_VALUE); // OP
      x = (String) vh.getAndSet(FIELD_VALUE); // OP
      x = (String) vh.getAndSet(FIELD_VALUE); // OP
      x = (String) vh.getAndSet(FIELD_VALUE); // OP
      x = (String) vh.getAndSet(FIELD_VALUE); // OP
      x = (String) vh.getAndSet(FIELD_VALUE); // OP
      x = (String) vh.getAndSet(FIELD_VALUE); // OP
      x = (String) vh.getAndSet(FIELD_VALUE); // OP
      x = (String) vh.getAndSet(FIELD_VALUE); // OP
      x = (String) vh.getAndSet(FIELD_VALUE); // OP
      x = (String) vh.getAndSet(FIELD_VALUE); // OP
      x = (String) vh.getAndSet(FIELD_VALUE); // OP
      x = (String) vh.getAndSet(FIELD_VALUE); // OP
      x = (String) vh.getAndSet(FIELD_VALUE); // OP
      x = (String) vh.getAndSet(FIELD_VALUE); // OP
      x = (String) vh.getAndSet(FIELD_VALUE); // OP
      x = (String) vh.getAndSet(FIELD_VALUE); // OP
    }
  }

  @Override
  public int innerIterations() {
      return 3000;
  }
}

class VarHandleGetAndSetFieldIntBenchmark extends MicroBenchmark {
  static final int FIELD_VALUE = 42;
  int field = ~FIELD_VALUE;
  VarHandle vh;

  VarHandleGetAndSetFieldIntBenchmark() throws Throwable {
    vh = MethodHandles.lookup().findVarHandle(this.getClass(), "field", int.class);
  }

  @Override
  public void run() {
    int x;
    for (int pass = 0; pass < 100; ++pass) {
      x = (int) vh.getAndSet(this, FIELD_VALUE); // OP
      x = (int) vh.getAndSet(this, FIELD_VALUE); // OP
      x = (int) vh.getAndSet(this, FIELD_VALUE); // OP
      x = (int) vh.getAndSet(this, FIELD_VALUE); // OP
      x = (int) vh.getAndSet(this, FIELD_VALUE); // OP
      x = (int) vh.getAndSet(this, FIELD_VALUE); // OP
      x = (int) vh.getAndSet(this, FIELD_VALUE); // OP
      x = (int) vh.getAndSet(this, FIELD_VALUE); // OP
      x = (int) vh.getAndSet(this, FIELD_VALUE); // OP
      x = (int) vh.getAndSet(this, FIELD_VALUE); // OP
      x = (int) vh.getAndSet(this, FIELD_VALUE); // OP
      x = (int) vh.getAndSet(this, FIELD_VALUE); // OP
      x = (int) vh.getAndSet(this, FIELD_VALUE); // OP
      x = (int) vh.getAndSet(this, FIELD_VALUE); // OP
      x = (int) vh.getAndSet(this, FIELD_VALUE); // OP
      x = (int) vh.getAndSet(this, FIELD_VALUE); // OP
      x = (int) vh.getAndSet(this, FIELD_VALUE); // OP
      x = (int) vh.getAndSet(this, FIELD_VALUE); // OP
      x = (int) vh.getAndSet(this, FIELD_VALUE); // OP
      x = (int) vh.getAndSet(this, FIELD_VALUE); // OP
      x = (int) vh.getAndSet(this, FIELD_VALUE); // OP
      x = (int) vh.getAndSet(this, FIELD_VALUE); // OP
      x = (int) vh.getAndSet(this, FIELD_VALUE); // OP
      x = (int) vh.getAndSet(this, FIELD_VALUE); // OP
      x = (int) vh.getAndSet(this, FIELD_VALUE); // OP
      x = (int) vh.getAndSet(this, FIELD_VALUE); // OP
      x = (int) vh.getAndSet(this, FIELD_VALUE); // OP
      x = (int) vh.getAndSet(this, FIELD_VALUE); // OP
      x = (int) vh.getAndSet(this, FIELD_VALUE); // OP
      x = (int) vh.getAndSet(this, FIELD_VALUE); // OP
    }
  }

  @Override
  public int innerIterations() {
      return 3000;
  }
}

class VarHandleGetAndSetFieldStringBenchmark extends MicroBenchmark {
  static final String FIELD_VALUE = "field value";
  String field = null;
  VarHandle vh;

  VarHandleGetAndSetFieldStringBenchmark() throws Throwable {
    vh = MethodHandles.lookup().findVarHandle(this.getClass(), "field", String.class);
  }

  @Override
  public void run() {
    String x;
    for (int pass = 0; pass < 100; ++pass) {
      x = (String) vh.getAndSet(this, FIELD_VALUE); // OP
      x = (String) vh.getAndSet(this, FIELD_VALUE); // OP
      x = (String) vh.getAndSet(this, FIELD_VALUE); // OP
      x = (String) vh.getAndSet(this, FIELD_VALUE); // OP
      x = (String) vh.getAndSet(this, FIELD_VALUE); // OP
      x = (String) vh.getAndSet(this, FIELD_VALUE); // OP
      x = (String) vh.getAndSet(this, FIELD_VALUE); // OP
      x = (String) vh.getAndSet(this, FIELD_VALUE); // OP
      x = (String) vh.getAndSet(this, FIELD_VALUE); // OP
      x = (String) vh.getAndSet(this, FIELD_VALUE); // OP
      x = (String) vh.getAndSet(this, FIELD_VALUE); // OP
      x = (String) vh.getAndSet(this, FIELD_VALUE); // OP
      x = (String) vh.getAndSet(this, FIELD_VALUE); // OP
      x = (String) vh.getAndSet(this, FIELD_VALUE); // OP
      x = (String) vh.getAndSet(this, FIELD_VALUE); // OP
      x = (String) vh.getAndSet(this, FIELD_VALUE); // OP
      x = (String) vh.getAndSet(this, FIELD_VALUE); // OP
      x = (String) vh.getAndSet(this, FIELD_VALUE); // OP
      x = (String) vh.getAndSet(this, FIELD_VALUE); // OP
      x = (String) vh.getAndSet(this, FIELD_VALUE); // OP
      x = (String) vh.getAndSet(this, FIELD_VALUE); // OP
      x = (String) vh.getAndSet(this, FIELD_VALUE); // OP
      x = (String) vh.getAndSet(this, FIELD_VALUE); // OP
      x = (String) vh.getAndSet(this, FIELD_VALUE); // OP
      x = (String) vh.getAndSet(this, FIELD_VALUE); // OP
      x = (String) vh.getAndSet(this, FIELD_VALUE); // OP
      x = (String) vh.getAndSet(this, FIELD_VALUE); // OP
      x = (String) vh.getAndSet(this, FIELD_VALUE); // OP
      x = (String) vh.getAndSet(this, FIELD_VALUE); // OP
      x = (String) vh.getAndSet(this, FIELD_VALUE); // OP
    }
  }

  @Override
  public int innerIterations() {
      return 3000;
  }
}

class VarHandleGetAndSetAcquireStaticFieldIntBenchmark extends MicroBenchmark {
  static final int FIELD_VALUE = 42;
  static int field = ~FIELD_VALUE;

  VarHandle vh;

  VarHandleGetAndSetAcquireStaticFieldIntBenchmark() throws Throwable {
    vh = MethodHandles.lookup().findStaticVarHandle(this.getClass(), "field", int.class);
  }

  @Override
  public void teardown() {
    if (field != FIELD_VALUE) {
      throw new RuntimeException("field has unexpected value: " + field);
    }
  }

  @Override
  public void run() {
    int x;
    for (int pass = 0; pass < 100; ++pass) {
      x = (int) vh.getAndSetAcquire(FIELD_VALUE); // OP
      x = (int) vh.getAndSetAcquire(FIELD_VALUE); // OP
      x = (int) vh.getAndSetAcquire(FIELD_VALUE); // OP
      x = (int) vh.getAndSetAcquire(FIELD_VALUE); // OP
      x = (int) vh.getAndSetAcquire(FIELD_VALUE); // OP
      x = (int) vh.getAndSetAcquire(FIELD_VALUE); // OP
      x = (int) vh.getAndSetAcquire(FIELD_VALUE); // OP
      x = (int) vh.getAndSetAcquire(FIELD_VALUE); // OP
      x = (int) vh.getAndSetAcquire(FIELD_VALUE); // OP
      x = (int) vh.getAndSetAcquire(FIELD_VALUE); // OP
      x = (int) vh.getAndSetAcquire(FIELD_VALUE); // OP
      x = (int) vh.getAndSetAcquire(FIELD_VALUE); // OP
      x = (int) vh.getAndSetAcquire(FIELD_VALUE); // OP
      x = (int) vh.getAndSetAcquire(FIELD_VALUE); // OP
      x = (int) vh.getAndSetAcquire(FIELD_VALUE); // OP
      x = (int) vh.getAndSetAcquire(FIELD_VALUE); // OP
      x = (int) vh.getAndSetAcquire(FIELD_VALUE); // OP
      x = (int) vh.getAndSetAcquire(FIELD_VALUE); // OP
      x = (int) vh.getAndSetAcquire(FIELD_VALUE); // OP
      x = (int) vh.getAndSetAcquire(FIELD_VALUE); // OP
      x = (int) vh.getAndSetAcquire(FIELD_VALUE); // OP
      x = (int) vh.getAndSetAcquire(FIELD_VALUE); // OP
      x = (int) vh.getAndSetAcquire(FIELD_VALUE); // OP
      x = (int) vh.getAndSetAcquire(FIELD_VALUE); // OP
      x = (int) vh.getAndSetAcquire(FIELD_VALUE); // OP
      x = (int) vh.getAndSetAcquire(FIELD_VALUE); // OP
      x = (int) vh.getAndSetAcquire(FIELD_VALUE); // OP
      x = (int) vh.getAndSetAcquire(FIELD_VALUE); // OP
      x = (int) vh.getAndSetAcquire(FIELD_VALUE); // OP
      x = (int) vh.getAndSetAcquire(FIELD_VALUE); // OP
    }
  }

  @Override
  public int innerIterations() {
      return 3000;
  }
}

class VarHandleGetAndSetAcquireStaticFieldStringBenchmark extends MicroBenchmark {
  static final String FIELD_VALUE = "a string";
  static String field = null;

  VarHandle vh;

  VarHandleGetAndSetAcquireStaticFieldStringBenchmark() throws Throwable {
    vh = MethodHandles.lookup().findStaticVarHandle(this.getClass(), "field", String.class);
  }

  @Override
  public void run() {
    String x;
    for (int pass = 0; pass < 100; ++pass) {
      x = (String) vh.getAndSetAcquire(FIELD_VALUE); // OP
      x = (String) vh.getAndSetAcquire(FIELD_VALUE); // OP
      x = (String) vh.getAndSetAcquire(FIELD_VALUE); // OP
      x = (String) vh.getAndSetAcquire(FIELD_VALUE); // OP
      x = (String) vh.getAndSetAcquire(FIELD_VALUE); // OP
      x = (String) vh.getAndSetAcquire(FIELD_VALUE); // OP
      x = (String) vh.getAndSetAcquire(FIELD_VALUE); // OP
      x = (String) vh.getAndSetAcquire(FIELD_VALUE); // OP
      x = (String) vh.getAndSetAcquire(FIELD_VALUE); // OP
      x = (String) vh.getAndSetAcquire(FIELD_VALUE); // OP
      x = (String) vh.getAndSetAcquire(FIELD_VALUE); // OP
      x = (String) vh.getAndSetAcquire(FIELD_VALUE); // OP
      x = (String) vh.getAndSetAcquire(FIELD_VALUE); // OP
      x = (String) vh.getAndSetAcquire(FIELD_VALUE); // OP
      x = (String) vh.getAndSetAcquire(FIELD_VALUE); // OP
      x = (String) vh.getAndSetAcquire(FIELD_VALUE); // OP
      x = (String) vh.getAndSetAcquire(FIELD_VALUE); // OP
      x = (String) vh.getAndSetAcquire(FIELD_VALUE); // OP
      x = (String) vh.getAndSetAcquire(FIELD_VALUE); // OP
      x = (String) vh.getAndSetAcquire(FIELD_VALUE); // OP
      x = (String) vh.getAndSetAcquire(FIELD_VALUE); // OP
      x = (String) vh.getAndSetAcquire(FIELD_VALUE); // OP
      x = (String) vh.getAndSetAcquire(FIELD_VALUE); // OP
      x = (String) vh.getAndSetAcquire(FIELD_VALUE); // OP
      x = (String) vh.getAndSetAcquire(FIELD_VALUE); // OP
      x = (String) vh.getAndSetAcquire(FIELD_VALUE); // OP
      x = (String) vh.getAndSetAcquire(FIELD_VALUE); // OP
      x = (String) vh.getAndSetAcquire(FIELD_VALUE); // OP
      x = (String) vh.getAndSetAcquire(FIELD_VALUE); // OP
      x = (String) vh.getAndSetAcquire(FIELD_VALUE); // OP
    }
  }

  @Override
  public int innerIterations() {
      return 3000;
  }
}

class VarHandleGetAndSetAcquireFieldIntBenchmark extends MicroBenchmark {
  static final int FIELD_VALUE = 42;
  int field = ~FIELD_VALUE;
  VarHandle vh;

  VarHandleGetAndSetAcquireFieldIntBenchmark() throws Throwable {
    vh = MethodHandles.lookup().findVarHandle(this.getClass(), "field", int.class);
  }

  @Override
  public void run() {
    int x;
    for (int pass = 0; pass < 100; ++pass) {
      x = (int) vh.getAndSetAcquire(this, FIELD_VALUE); // OP
      x = (int) vh.getAndSetAcquire(this, FIELD_VALUE); // OP
      x = (int) vh.getAndSetAcquire(this, FIELD_VALUE); // OP
      x = (int) vh.getAndSetAcquire(this, FIELD_VALUE); // OP
      x = (int) vh.getAndSetAcquire(this, FIELD_VALUE); // OP
      x = (int) vh.getAndSetAcquire(this, FIELD_VALUE); // OP
      x = (int) vh.getAndSetAcquire(this, FIELD_VALUE); // OP
      x = (int) vh.getAndSetAcquire(this, FIELD_VALUE); // OP
      x = (int) vh.getAndSetAcquire(this, FIELD_VALUE); // OP
      x = (int) vh.getAndSetAcquire(this, FIELD_VALUE); // OP
      x = (int) vh.getAndSetAcquire(this, FIELD_VALUE); // OP
      x = (int) vh.getAndSetAcquire(this, FIELD_VALUE); // OP
      x = (int) vh.getAndSetAcquire(this, FIELD_VALUE); // OP
      x = (int) vh.getAndSetAcquire(this, FIELD_VALUE); // OP
      x = (int) vh.getAndSetAcquire(this, FIELD_VALUE); // OP
      x = (int) vh.getAndSetAcquire(this, FIELD_VALUE); // OP
      x = (int) vh.getAndSetAcquire(this, FIELD_VALUE); // OP
      x = (int) vh.getAndSetAcquire(this, FIELD_VALUE); // OP
      x = (int) vh.getAndSetAcquire(this, FIELD_VALUE); // OP
      x = (int) vh.getAndSetAcquire(this, FIELD_VALUE); // OP
      x = (int) vh.getAndSetAcquire(this, FIELD_VALUE); // OP
      x = (int) vh.getAndSetAcquire(this, FIELD_VALUE); // OP
      x = (int) vh.getAndSetAcquire(this, FIELD_VALUE); // OP
      x = (int) vh.getAndSetAcquire(this, FIELD_VALUE); // OP
      x = (int) vh.getAndSetAcquire(this, FIELD_VALUE); // OP
      x = (int) vh.getAndSetAcquire(this, FIELD_VALUE); // OP
      x = (int) vh.getAndSetAcquire(this, FIELD_VALUE); // OP
      x = (int) vh.getAndSetAcquire(this, FIELD_VALUE); // OP
      x = (int) vh.getAndSetAcquire(this, FIELD_VALUE); // OP
      x = (int) vh.getAndSetAcquire(this, FIELD_VALUE); // OP
    }
  }

  @Override
  public int innerIterations() {
      return 3000;
  }
}

class VarHandleGetAndSetAcquireFieldStringBenchmark extends MicroBenchmark {
  static final String FIELD_VALUE = "field value";
  String field = null;
  VarHandle vh;

  VarHandleGetAndSetAcquireFieldStringBenchmark() throws Throwable {
    vh = MethodHandles.lookup().findVarHandle(this.getClass(), "field", String.class);
  }

  @Override
  public void run() {
    String x;
    for (int pass = 0; pass < 100; ++pass) {
      x = (String) vh.getAndSetAcquire(this, FIELD_VALUE); // OP
      x = (String) vh.getAndSetAcquire(this, FIELD_VALUE); // OP
      x = (String) vh.getAndSetAcquire(this, FIELD_VALUE); // OP
      x = (String) vh.getAndSetAcquire(this, FIELD_VALUE); // OP
      x = (String) vh.getAndSetAcquire(this, FIELD_VALUE); // OP
      x = (String) vh.getAndSetAcquire(this, FIELD_VALUE); // OP
      x = (String) vh.getAndSetAcquire(this, FIELD_VALUE); // OP
      x = (String) vh.getAndSetAcquire(this, FIELD_VALUE); // OP
      x = (String) vh.getAndSetAcquire(this, FIELD_VALUE); // OP
      x = (String) vh.getAndSetAcquire(this, FIELD_VALUE); // OP
      x = (String) vh.getAndSetAcquire(this, FIELD_VALUE); // OP
      x = (String) vh.getAndSetAcquire(this, FIELD_VALUE); // OP
      x = (String) vh.getAndSetAcquire(this, FIELD_VALUE); // OP
      x = (String) vh.getAndSetAcquire(this, FIELD_VALUE); // OP
      x = (String) vh.getAndSetAcquire(this, FIELD_VALUE); // OP
      x = (String) vh.getAndSetAcquire(this, FIELD_VALUE); // OP
      x = (String) vh.getAndSetAcquire(this, FIELD_VALUE); // OP
      x = (String) vh.getAndSetAcquire(this, FIELD_VALUE); // OP
      x = (String) vh.getAndSetAcquire(this, FIELD_VALUE); // OP
      x = (String) vh.getAndSetAcquire(this, FIELD_VALUE); // OP
      x = (String) vh.getAndSetAcquire(this, FIELD_VALUE); // OP
      x = (String) vh.getAndSetAcquire(this, FIELD_VALUE); // OP
      x = (String) vh.getAndSetAcquire(this, FIELD_VALUE); // OP
      x = (String) vh.getAndSetAcquire(this, FIELD_VALUE); // OP
      x = (String) vh.getAndSetAcquire(this, FIELD_VALUE); // OP
      x = (String) vh.getAndSetAcquire(this, FIELD_VALUE); // OP
      x = (String) vh.getAndSetAcquire(this, FIELD_VALUE); // OP
      x = (String) vh.getAndSetAcquire(this, FIELD_VALUE); // OP
      x = (String) vh.getAndSetAcquire(this, FIELD_VALUE); // OP
      x = (String) vh.getAndSetAcquire(this, FIELD_VALUE); // OP
    }
  }

  @Override
  public int innerIterations() {
      return 3000;
  }
}

class VarHandleGetAndSetReleaseStaticFieldIntBenchmark extends MicroBenchmark {
  static final int FIELD_VALUE = 42;
  static int field = ~FIELD_VALUE;

  VarHandle vh;

  VarHandleGetAndSetReleaseStaticFieldIntBenchmark() throws Throwable {
    vh = MethodHandles.lookup().findStaticVarHandle(this.getClass(), "field", int.class);
  }

  @Override
  public void teardown() {
    if (field != FIELD_VALUE) {
      throw new RuntimeException("field has unexpected value: " + field);
    }
  }

  @Override
  public void run() {
    int x;
    for (int pass = 0; pass < 100; ++pass) {
      x = (int) vh.getAndSetRelease(FIELD_VALUE); // OP
      x = (int) vh.getAndSetRelease(FIELD_VALUE); // OP
      x = (int) vh.getAndSetRelease(FIELD_VALUE); // OP
      x = (int) vh.getAndSetRelease(FIELD_VALUE); // OP
      x = (int) vh.getAndSetRelease(FIELD_VALUE); // OP
      x = (int) vh.getAndSetRelease(FIELD_VALUE); // OP
      x = (int) vh.getAndSetRelease(FIELD_VALUE); // OP
      x = (int) vh.getAndSetRelease(FIELD_VALUE); // OP
      x = (int) vh.getAndSetRelease(FIELD_VALUE); // OP
      x = (int) vh.getAndSetRelease(FIELD_VALUE); // OP
      x = (int) vh.getAndSetRelease(FIELD_VALUE); // OP
      x = (int) vh.getAndSetRelease(FIELD_VALUE); // OP
      x = (int) vh.getAndSetRelease(FIELD_VALUE); // OP
      x = (int) vh.getAndSetRelease(FIELD_VALUE); // OP
      x = (int) vh.getAndSetRelease(FIELD_VALUE); // OP
      x = (int) vh.getAndSetRelease(FIELD_VALUE); // OP
      x = (int) vh.getAndSetRelease(FIELD_VALUE); // OP
      x = (int) vh.getAndSetRelease(FIELD_VALUE); // OP
      x = (int) vh.getAndSetRelease(FIELD_VALUE); // OP
      x = (int) vh.getAndSetRelease(FIELD_VALUE); // OP
      x = (int) vh.getAndSetRelease(FIELD_VALUE); // OP
      x = (int) vh.getAndSetRelease(FIELD_VALUE); // OP
      x = (int) vh.getAndSetRelease(FIELD_VALUE); // OP
      x = (int) vh.getAndSetRelease(FIELD_VALUE); // OP
      x = (int) vh.getAndSetRelease(FIELD_VALUE); // OP
      x = (int) vh.getAndSetRelease(FIELD_VALUE); // OP
      x = (int) vh.getAndSetRelease(FIELD_VALUE); // OP
      x = (int) vh.getAndSetRelease(FIELD_VALUE); // OP
      x = (int) vh.getAndSetRelease(FIELD_VALUE); // OP
      x = (int) vh.getAndSetRelease(FIELD_VALUE); // OP
    }
  }

  @Override
  public int innerIterations() {
      return 3000;
  }
}

class VarHandleGetAndSetReleaseStaticFieldStringBenchmark extends MicroBenchmark {
  static final String FIELD_VALUE = "a string";
  static String field = null;

  VarHandle vh;

  VarHandleGetAndSetReleaseStaticFieldStringBenchmark() throws Throwable {
    vh = MethodHandles.lookup().findStaticVarHandle(this.getClass(), "field", String.class);
  }

  @Override
  public void run() {
    String x;
    for (int pass = 0; pass < 100; ++pass) {
      x = (String) vh.getAndSetRelease(FIELD_VALUE); // OP
      x = (String) vh.getAndSetRelease(FIELD_VALUE); // OP
      x = (String) vh.getAndSetRelease(FIELD_VALUE); // OP
      x = (String) vh.getAndSetRelease(FIELD_VALUE); // OP
      x = (String) vh.getAndSetRelease(FIELD_VALUE); // OP
      x = (String) vh.getAndSetRelease(FIELD_VALUE); // OP
      x = (String) vh.getAndSetRelease(FIELD_VALUE); // OP
      x = (String) vh.getAndSetRelease(FIELD_VALUE); // OP
      x = (String) vh.getAndSetRelease(FIELD_VALUE); // OP
      x = (String) vh.getAndSetRelease(FIELD_VALUE); // OP
      x = (String) vh.getAndSetRelease(FIELD_VALUE); // OP
      x = (String) vh.getAndSetRelease(FIELD_VALUE); // OP
      x = (String) vh.getAndSetRelease(FIELD_VALUE); // OP
      x = (String) vh.getAndSetRelease(FIELD_VALUE); // OP
      x = (String) vh.getAndSetRelease(FIELD_VALUE); // OP
      x = (String) vh.getAndSetRelease(FIELD_VALUE); // OP
      x = (String) vh.getAndSetRelease(FIELD_VALUE); // OP
      x = (String) vh.getAndSetRelease(FIELD_VALUE); // OP
      x = (String) vh.getAndSetRelease(FIELD_VALUE); // OP
      x = (String) vh.getAndSetRelease(FIELD_VALUE); // OP
      x = (String) vh.getAndSetRelease(FIELD_VALUE); // OP
      x = (String) vh.getAndSetRelease(FIELD_VALUE); // OP
      x = (String) vh.getAndSetRelease(FIELD_VALUE); // OP
      x = (String) vh.getAndSetRelease(FIELD_VALUE); // OP
      x = (String) vh.getAndSetRelease(FIELD_VALUE); // OP
      x = (String) vh.getAndSetRelease(FIELD_VALUE); // OP
      x = (String) vh.getAndSetRelease(FIELD_VALUE); // OP
      x = (String) vh.getAndSetRelease(FIELD_VALUE); // OP
      x = (String) vh.getAndSetRelease(FIELD_VALUE); // OP
      x = (String) vh.getAndSetRelease(FIELD_VALUE); // OP
    }
  }

  @Override
  public int innerIterations() {
      return 3000;
  }
}

class VarHandleGetAndSetReleaseFieldIntBenchmark extends MicroBenchmark {
  static final int FIELD_VALUE = 42;
  int field = ~FIELD_VALUE;
  VarHandle vh;

  VarHandleGetAndSetReleaseFieldIntBenchmark() throws Throwable {
    vh = MethodHandles.lookup().findVarHandle(this.getClass(), "field", int.class);
  }

  @Override
  public void run() {
    int x;
    for (int pass = 0; pass < 100; ++pass) {
      x = (int) vh.getAndSetRelease(this, FIELD_VALUE); // OP
      x = (int) vh.getAndSetRelease(this, FIELD_VALUE); // OP
      x = (int) vh.getAndSetRelease(this, FIELD_VALUE); // OP
      x = (int) vh.getAndSetRelease(this, FIELD_VALUE); // OP
      x = (int) vh.getAndSetRelease(this, FIELD_VALUE); // OP
      x = (int) vh.getAndSetRelease(this, FIELD_VALUE); // OP
      x = (int) vh.getAndSetRelease(this, FIELD_VALUE); // OP
      x = (int) vh.getAndSetRelease(this, FIELD_VALUE); // OP
      x = (int) vh.getAndSetRelease(this, FIELD_VALUE); // OP
      x = (int) vh.getAndSetRelease(this, FIELD_VALUE); // OP
      x = (int) vh.getAndSetRelease(this, FIELD_VALUE); // OP
      x = (int) vh.getAndSetRelease(this, FIELD_VALUE); // OP
      x = (int) vh.getAndSetRelease(this, FIELD_VALUE); // OP
      x = (int) vh.getAndSetRelease(this, FIELD_VALUE); // OP
      x = (int) vh.getAndSetRelease(this, FIELD_VALUE); // OP
      x = (int) vh.getAndSetRelease(this, FIELD_VALUE); // OP
      x = (int) vh.getAndSetRelease(this, FIELD_VALUE); // OP
      x = (int) vh.getAndSetRelease(this, FIELD_VALUE); // OP
      x = (int) vh.getAndSetRelease(this, FIELD_VALUE); // OP
      x = (int) vh.getAndSetRelease(this, FIELD_VALUE); // OP
      x = (int) vh.getAndSetRelease(this, FIELD_VALUE); // OP
      x = (int) vh.getAndSetRelease(this, FIELD_VALUE); // OP
      x = (int) vh.getAndSetRelease(this, FIELD_VALUE); // OP
      x = (int) vh.getAndSetRelease(this, FIELD_VALUE); // OP
      x = (int) vh.getAndSetRelease(this, FIELD_VALUE); // OP
      x = (int) vh.getAndSetRelease(this, FIELD_VALUE); // OP
      x = (int) vh.getAndSetRelease(this, FIELD_VALUE); // OP
      x = (int) vh.getAndSetRelease(this, FIELD_VALUE); // OP
      x = (int) vh.getAndSetRelease(this, FIELD_VALUE); // OP
      x = (int) vh.getAndSetRelease(this, FIELD_VALUE); // OP
    }
  }

  @Override
  public int innerIterations() {
      return 3000;
  }
}

class VarHandleGetAndSetReleaseFieldStringBenchmark extends MicroBenchmark {
  static final String FIELD_VALUE = "field value";
  String field = null;
  VarHandle vh;

  VarHandleGetAndSetReleaseFieldStringBenchmark() throws Throwable {
    vh = MethodHandles.lookup().findVarHandle(this.getClass(), "field", String.class);
  }

  @Override
  public void run() {
    String x;
    for (int pass = 0; pass < 100; ++pass) {
      x = (String) vh.getAndSetRelease(this, FIELD_VALUE); // OP
      x = (String) vh.getAndSetRelease(this, FIELD_VALUE); // OP
      x = (String) vh.getAndSetRelease(this, FIELD_VALUE); // OP
      x = (String) vh.getAndSetRelease(this, FIELD_VALUE); // OP
      x = (String) vh.getAndSetRelease(this, FIELD_VALUE); // OP
      x = (String) vh.getAndSetRelease(this, FIELD_VALUE); // OP
      x = (String) vh.getAndSetRelease(this, FIELD_VALUE); // OP
      x = (String) vh.getAndSetRelease(this, FIELD_VALUE); // OP
      x = (String) vh.getAndSetRelease(this, FIELD_VALUE); // OP
      x = (String) vh.getAndSetRelease(this, FIELD_VALUE); // OP
      x = (String) vh.getAndSetRelease(this, FIELD_VALUE); // OP
      x = (String) vh.getAndSetRelease(this, FIELD_VALUE); // OP
      x = (String) vh.getAndSetRelease(this, FIELD_VALUE); // OP
      x = (String) vh.getAndSetRelease(this, FIELD_VALUE); // OP
      x = (String) vh.getAndSetRelease(this, FIELD_VALUE); // OP
      x = (String) vh.getAndSetRelease(this, FIELD_VALUE); // OP
      x = (String) vh.getAndSetRelease(this, FIELD_VALUE); // OP
      x = (String) vh.getAndSetRelease(this, FIELD_VALUE); // OP
      x = (String) vh.getAndSetRelease(this, FIELD_VALUE); // OP
      x = (String) vh.getAndSetRelease(this, FIELD_VALUE); // OP
      x = (String) vh.getAndSetRelease(this, FIELD_VALUE); // OP
      x = (String) vh.getAndSetRelease(this, FIELD_VALUE); // OP
      x = (String) vh.getAndSetRelease(this, FIELD_VALUE); // OP
      x = (String) vh.getAndSetRelease(this, FIELD_VALUE); // OP
      x = (String) vh.getAndSetRelease(this, FIELD_VALUE); // OP
      x = (String) vh.getAndSetRelease(this, FIELD_VALUE); // OP
      x = (String) vh.getAndSetRelease(this, FIELD_VALUE); // OP
      x = (String) vh.getAndSetRelease(this, FIELD_VALUE); // OP
      x = (String) vh.getAndSetRelease(this, FIELD_VALUE); // OP
      x = (String) vh.getAndSetRelease(this, FIELD_VALUE); // OP
    }
  }

  @Override
  public int innerIterations() {
      return 3000;
  }
}

class VarHandleCompareAndExchangeStaticFieldIntBenchmark extends MicroBenchmark {
  static final int FIELD_VALUE = 42;
  static int field = FIELD_VALUE;

  VarHandle vh;

  VarHandleCompareAndExchangeStaticFieldIntBenchmark() throws Throwable {
    vh = MethodHandles.lookup().findStaticVarHandle(this.getClass(), "field", int.class);
  }

  @Override
  public void run() {
    int newValue = (field != 0) ? 0 : FIELD_VALUE;
    int result;
    for (int pass = 0; pass < 100; ++pass) {
      result = (int) vh.compareAndExchange(field, newValue); // OP
      result = (int) vh.compareAndExchange(field, newValue); // OP
      result = (int) vh.compareAndExchange(field, newValue); // OP
      result = (int) vh.compareAndExchange(field, newValue); // OP
      result = (int) vh.compareAndExchange(field, newValue); // OP
      result = (int) vh.compareAndExchange(field, newValue); // OP
      result = (int) vh.compareAndExchange(field, newValue); // OP
      result = (int) vh.compareAndExchange(field, newValue); // OP
      result = (int) vh.compareAndExchange(field, newValue); // OP
      result = (int) vh.compareAndExchange(field, newValue); // OP
      result = (int) vh.compareAndExchange(field, newValue); // OP
      result = (int) vh.compareAndExchange(field, newValue); // OP
      result = (int) vh.compareAndExchange(field, newValue); // OP
      result = (int) vh.compareAndExchange(field, newValue); // OP
      result = (int) vh.compareAndExchange(field, newValue); // OP
      result = (int) vh.compareAndExchange(field, newValue); // OP
      result = (int) vh.compareAndExchange(field, newValue); // OP
      result = (int) vh.compareAndExchange(field, newValue); // OP
      result = (int) vh.compareAndExchange(field, newValue); // OP
      result = (int) vh.compareAndExchange(field, newValue); // OP
      result = (int) vh.compareAndExchange(field, newValue); // OP
      result = (int) vh.compareAndExchange(field, newValue); // OP
      result = (int) vh.compareAndExchange(field, newValue); // OP
      result = (int) vh.compareAndExchange(field, newValue); // OP
      result = (int) vh.compareAndExchange(field, newValue); // OP
      result = (int) vh.compareAndExchange(field, newValue); // OP
      result = (int) vh.compareAndExchange(field, newValue); // OP
      result = (int) vh.compareAndExchange(field, newValue); // OP
      result = (int) vh.compareAndExchange(field, newValue); // OP
      result = (int) vh.compareAndExchange(field, newValue); // OP
    }
  }

  @Override
  public int innerIterations() {
      return 3000;
  }
}

class VarHandleCompareAndExchangeStaticFieldStringBenchmark extends MicroBenchmark {
  static final String FIELD_VALUE = "a string";
  static String field = null;

  VarHandle vh;

  VarHandleCompareAndExchangeStaticFieldStringBenchmark() throws Throwable {
    vh = MethodHandles.lookup().findStaticVarHandle(this.getClass(), "field", String.class);
  }

  @Override
  public void run() {
    String newValue = (field != null) ? null : FIELD_VALUE;
    String result;
    for (int pass = 0; pass < 100; ++pass) {
      result = (String) vh.compareAndExchange(field, newValue); // OP
      result = (String) vh.compareAndExchange(field, newValue); // OP
      result = (String) vh.compareAndExchange(field, newValue); // OP
      result = (String) vh.compareAndExchange(field, newValue); // OP
      result = (String) vh.compareAndExchange(field, newValue); // OP
      result = (String) vh.compareAndExchange(field, newValue); // OP
      result = (String) vh.compareAndExchange(field, newValue); // OP
      result = (String) vh.compareAndExchange(field, newValue); // OP
      result = (String) vh.compareAndExchange(field, newValue); // OP
      result = (String) vh.compareAndExchange(field, newValue); // OP
      result = (String) vh.compareAndExchange(field, newValue); // OP
      result = (String) vh.compareAndExchange(field, newValue); // OP
      result = (String) vh.compareAndExchange(field, newValue); // OP
      result = (String) vh.compareAndExchange(field, newValue); // OP
      result = (String) vh.compareAndExchange(field, newValue); // OP
      result = (String) vh.compareAndExchange(field, newValue); // OP
      result = (String) vh.compareAndExchange(field, newValue); // OP
      result = (String) vh.compareAndExchange(field, newValue); // OP
      result = (String) vh.compareAndExchange(field, newValue); // OP
      result = (String) vh.compareAndExchange(field, newValue); // OP
      result = (String) vh.compareAndExchange(field, newValue); // OP
      result = (String) vh.compareAndExchange(field, newValue); // OP
      result = (String) vh.compareAndExchange(field, newValue); // OP
      result = (String) vh.compareAndExchange(field, newValue); // OP
      result = (String) vh.compareAndExchange(field, newValue); // OP
      result = (String) vh.compareAndExchange(field, newValue); // OP
      result = (String) vh.compareAndExchange(field, newValue); // OP
      result = (String) vh.compareAndExchange(field, newValue); // OP
      result = (String) vh.compareAndExchange(field, newValue); // OP
      result = (String) vh.compareAndExchange(field, newValue); // OP
    }
  }

  @Override
  public int innerIterations() {
      return 3000;
  }
}

class VarHandleCompareAndExchangeFieldIntBenchmark extends MicroBenchmark {
  static final int FIELD_VALUE = 42;

  int field = FIELD_VALUE;
  VarHandle vh;

  VarHandleCompareAndExchangeFieldIntBenchmark() throws Throwable {
    vh = MethodHandles.lookup().findVarHandle(this.getClass(), "field", int.class);
  }

  @Override
  public void run() {
    int newValue = (field != 0) ? 0 : FIELD_VALUE;
    int result;
    for (int pass = 0; pass < 100; ++pass) {
      result = (int) vh.compareAndExchange(this, field, newValue); // OP
      result = (int) vh.compareAndExchange(this, field, newValue); // OP
      result = (int) vh.compareAndExchange(this, field, newValue); // OP
      result = (int) vh.compareAndExchange(this, field, newValue); // OP
      result = (int) vh.compareAndExchange(this, field, newValue); // OP
      result = (int) vh.compareAndExchange(this, field, newValue); // OP
      result = (int) vh.compareAndExchange(this, field, newValue); // OP
      result = (int) vh.compareAndExchange(this, field, newValue); // OP
      result = (int) vh.compareAndExchange(this, field, newValue); // OP
      result = (int) vh.compareAndExchange(this, field, newValue); // OP
      result = (int) vh.compareAndExchange(this, field, newValue); // OP
      result = (int) vh.compareAndExchange(this, field, newValue); // OP
      result = (int) vh.compareAndExchange(this, field, newValue); // OP
      result = (int) vh.compareAndExchange(this, field, newValue); // OP
      result = (int) vh.compareAndExchange(this, field, newValue); // OP
      result = (int) vh.compareAndExchange(this, field, newValue); // OP
      result = (int) vh.compareAndExchange(this, field, newValue); // OP
      result = (int) vh.compareAndExchange(this, field, newValue); // OP
      result = (int) vh.compareAndExchange(this, field, newValue); // OP
      result = (int) vh.compareAndExchange(this, field, newValue); // OP
      result = (int) vh.compareAndExchange(this, field, newValue); // OP
      result = (int) vh.compareAndExchange(this, field, newValue); // OP
      result = (int) vh.compareAndExchange(this, field, newValue); // OP
      result = (int) vh.compareAndExchange(this, field, newValue); // OP
      result = (int) vh.compareAndExchange(this, field, newValue); // OP
      result = (int) vh.compareAndExchange(this, field, newValue); // OP
      result = (int) vh.compareAndExchange(this, field, newValue); // OP
      result = (int) vh.compareAndExchange(this, field, newValue); // OP
      result = (int) vh.compareAndExchange(this, field, newValue); // OP
      result = (int) vh.compareAndExchange(this, field, newValue); // OP
    }
  }

  @Override
  public int innerIterations() {
      return 3000;
  }
}

class VarHandleCompareAndExchangeFieldStringBenchmark extends MicroBenchmark {
  static final String FIELD_VALUE = "field value";
  String field = null;
  VarHandle vh;

  VarHandleCompareAndExchangeFieldStringBenchmark() throws Throwable {
    vh = MethodHandles.lookup().findVarHandle(this.getClass(), "field", String.class);
  }

  @Override
  public void run() {
    String newValue = (field != null) ? null : FIELD_VALUE;
    String result;
    for (int pass = 0; pass < 100; ++pass) {
      result = (String) vh.compareAndExchange(this, field, newValue); // OP
      result = (String) vh.compareAndExchange(this, field, newValue); // OP
      result = (String) vh.compareAndExchange(this, field, newValue); // OP
      result = (String) vh.compareAndExchange(this, field, newValue); // OP
      result = (String) vh.compareAndExchange(this, field, newValue); // OP
      result = (String) vh.compareAndExchange(this, field, newValue); // OP
      result = (String) vh.compareAndExchange(this, field, newValue); // OP
      result = (String) vh.compareAndExchange(this, field, newValue); // OP
      result = (String) vh.compareAndExchange(this, field, newValue); // OP
      result = (String) vh.compareAndExchange(this, field, newValue); // OP
      result = (String) vh.compareAndExchange(this, field, newValue); // OP
      result = (String) vh.compareAndExchange(this, field, newValue); // OP
      result = (String) vh.compareAndExchange(this, field, newValue); // OP
      result = (String) vh.compareAndExchange(this, field, newValue); // OP
      result = (String) vh.compareAndExchange(this, field, newValue); // OP
      result = (String) vh.compareAndExchange(this, field, newValue); // OP
      result = (String) vh.compareAndExchange(this, field, newValue); // OP
      result = (String) vh.compareAndExchange(this, field, newValue); // OP
      result = (String) vh.compareAndExchange(this, field, newValue); // OP
      result = (String) vh.compareAndExchange(this, field, newValue); // OP
      result = (String) vh.compareAndExchange(this, field, newValue); // OP
      result = (String) vh.compareAndExchange(this, field, newValue); // OP
      result = (String) vh.compareAndExchange(this, field, newValue); // OP
      result = (String) vh.compareAndExchange(this, field, newValue); // OP
      result = (String) vh.compareAndExchange(this, field, newValue); // OP
      result = (String) vh.compareAndExchange(this, field, newValue); // OP
      result = (String) vh.compareAndExchange(this, field, newValue); // OP
      result = (String) vh.compareAndExchange(this, field, newValue); // OP
      result = (String) vh.compareAndExchange(this, field, newValue); // OP
      result = (String) vh.compareAndExchange(this, field, newValue); // OP
    }
  }

  @Override
  public int innerIterations() {
      return 3000;
  }
}

class VarHandleCompareAndExchangeAcquireStaticFieldIntBenchmark extends MicroBenchmark {
  static final int FIELD_VALUE = 42;
  static int field = FIELD_VALUE;

  VarHandle vh;

  VarHandleCompareAndExchangeAcquireStaticFieldIntBenchmark() throws Throwable {
    vh = MethodHandles.lookup().findStaticVarHandle(this.getClass(), "field", int.class);
  }

  @Override
  public void run() {
    int newValue = (field != 0) ? 0 : FIELD_VALUE;
    int result;
    for (int pass = 0; pass < 100; ++pass) {
      result = (int) vh.compareAndExchangeAcquire(field, newValue); // OP
      result = (int) vh.compareAndExchangeAcquire(field, newValue); // OP
      result = (int) vh.compareAndExchangeAcquire(field, newValue); // OP
      result = (int) vh.compareAndExchangeAcquire(field, newValue); // OP
      result = (int) vh.compareAndExchangeAcquire(field, newValue); // OP
      result = (int) vh.compareAndExchangeAcquire(field, newValue); // OP
      result = (int) vh.compareAndExchangeAcquire(field, newValue); // OP
      result = (int) vh.compareAndExchangeAcquire(field, newValue); // OP
      result = (int) vh.compareAndExchangeAcquire(field, newValue); // OP
      result = (int) vh.compareAndExchangeAcquire(field, newValue); // OP
      result = (int) vh.compareAndExchangeAcquire(field, newValue); // OP
      result = (int) vh.compareAndExchangeAcquire(field, newValue); // OP
      result = (int) vh.compareAndExchangeAcquire(field, newValue); // OP
      result = (int) vh.compareAndExchangeAcquire(field, newValue); // OP
      result = (int) vh.compareAndExchangeAcquire(field, newValue); // OP
      result = (int) vh.compareAndExchangeAcquire(field, newValue); // OP
      result = (int) vh.compareAndExchangeAcquire(field, newValue); // OP
      result = (int) vh.compareAndExchangeAcquire(field, newValue); // OP
      result = (int) vh.compareAndExchangeAcquire(field, newValue); // OP
      result = (int) vh.compareAndExchangeAcquire(field, newValue); // OP
      result = (int) vh.compareAndExchangeAcquire(field, newValue); // OP
      result = (int) vh.compareAndExchangeAcquire(field, newValue); // OP
      result = (int) vh.compareAndExchangeAcquire(field, newValue); // OP
      result = (int) vh.compareAndExchangeAcquire(field, newValue); // OP
      result = (int) vh.compareAndExchangeAcquire(field, newValue); // OP
      result = (int) vh.compareAndExchangeAcquire(field, newValue); // OP
      result = (int) vh.compareAndExchangeAcquire(field, newValue); // OP
      result = (int) vh.compareAndExchangeAcquire(field, newValue); // OP
      result = (int) vh.compareAndExchangeAcquire(field, newValue); // OP
      result = (int) vh.compareAndExchangeAcquire(field, newValue); // OP
    }
  }

  @Override
  public int innerIterations() {
      return 3000;
  }
}

class VarHandleCompareAndExchangeAcquireStaticFieldStringBenchmark extends MicroBenchmark {
  static final String FIELD_VALUE = "a string";
  static String field = null;

  VarHandle vh;

  VarHandleCompareAndExchangeAcquireStaticFieldStringBenchmark() throws Throwable {
    vh = MethodHandles.lookup().findStaticVarHandle(this.getClass(), "field", String.class);
  }

  @Override
  public void run() {
    String newValue = (field != null) ? null : FIELD_VALUE;
    String result;
    for (int pass = 0; pass < 100; ++pass) {
      result = (String) vh.compareAndExchangeAcquire(field, newValue); // OP
      result = (String) vh.compareAndExchangeAcquire(field, newValue); // OP
      result = (String) vh.compareAndExchangeAcquire(field, newValue); // OP
      result = (String) vh.compareAndExchangeAcquire(field, newValue); // OP
      result = (String) vh.compareAndExchangeAcquire(field, newValue); // OP
      result = (String) vh.compareAndExchangeAcquire(field, newValue); // OP
      result = (String) vh.compareAndExchangeAcquire(field, newValue); // OP
      result = (String) vh.compareAndExchangeAcquire(field, newValue); // OP
      result = (String) vh.compareAndExchangeAcquire(field, newValue); // OP
      result = (String) vh.compareAndExchangeAcquire(field, newValue); // OP
      result = (String) vh.compareAndExchangeAcquire(field, newValue); // OP
      result = (String) vh.compareAndExchangeAcquire(field, newValue); // OP
      result = (String) vh.compareAndExchangeAcquire(field, newValue); // OP
      result = (String) vh.compareAndExchangeAcquire(field, newValue); // OP
      result = (String) vh.compareAndExchangeAcquire(field, newValue); // OP
      result = (String) vh.compareAndExchangeAcquire(field, newValue); // OP
      result = (String) vh.compareAndExchangeAcquire(field, newValue); // OP
      result = (String) vh.compareAndExchangeAcquire(field, newValue); // OP
      result = (String) vh.compareAndExchangeAcquire(field, newValue); // OP
      result = (String) vh.compareAndExchangeAcquire(field, newValue); // OP
      result = (String) vh.compareAndExchangeAcquire(field, newValue); // OP
      result = (String) vh.compareAndExchangeAcquire(field, newValue); // OP
      result = (String) vh.compareAndExchangeAcquire(field, newValue); // OP
      result = (String) vh.compareAndExchangeAcquire(field, newValue); // OP
      result = (String) vh.compareAndExchangeAcquire(field, newValue); // OP
      result = (String) vh.compareAndExchangeAcquire(field, newValue); // OP
      result = (String) vh.compareAndExchangeAcquire(field, newValue); // OP
      result = (String) vh.compareAndExchangeAcquire(field, newValue); // OP
      result = (String) vh.compareAndExchangeAcquire(field, newValue); // OP
      result = (String) vh.compareAndExchangeAcquire(field, newValue); // OP
    }
  }

  @Override
  public int innerIterations() {
      return 3000;
  }
}

class VarHandleCompareAndExchangeAcquireFieldIntBenchmark extends MicroBenchmark {
  static final int FIELD_VALUE = 42;

  int field = FIELD_VALUE;
  VarHandle vh;

  VarHandleCompareAndExchangeAcquireFieldIntBenchmark() throws Throwable {
    vh = MethodHandles.lookup().findVarHandle(this.getClass(), "field", int.class);
  }

  @Override
  public void run() {
    int newValue = (field != 0) ? 0 : FIELD_VALUE;
    int result;
    for (int pass = 0; pass < 100; ++pass) {
      result = (int) vh.compareAndExchangeAcquire(this, field, newValue); // OP
      result = (int) vh.compareAndExchangeAcquire(this, field, newValue); // OP
      result = (int) vh.compareAndExchangeAcquire(this, field, newValue); // OP
      result = (int) vh.compareAndExchangeAcquire(this, field, newValue); // OP
      result = (int) vh.compareAndExchangeAcquire(this, field, newValue); // OP
      result = (int) vh.compareAndExchangeAcquire(this, field, newValue); // OP
      result = (int) vh.compareAndExchangeAcquire(this, field, newValue); // OP
      result = (int) vh.compareAndExchangeAcquire(this, field, newValue); // OP
      result = (int) vh.compareAndExchangeAcquire(this, field, newValue); // OP
      result = (int) vh.compareAndExchangeAcquire(this, field, newValue); // OP
      result = (int) vh.compareAndExchangeAcquire(this, field, newValue); // OP
      result = (int) vh.compareAndExchangeAcquire(this, field, newValue); // OP
      result = (int) vh.compareAndExchangeAcquire(this, field, newValue); // OP
      result = (int) vh.compareAndExchangeAcquire(this, field, newValue); // OP
      result = (int) vh.compareAndExchangeAcquire(this, field, newValue); // OP
      result = (int) vh.compareAndExchangeAcquire(this, field, newValue); // OP
      result = (int) vh.compareAndExchangeAcquire(this, field, newValue); // OP
      result = (int) vh.compareAndExchangeAcquire(this, field, newValue); // OP
      result = (int) vh.compareAndExchangeAcquire(this, field, newValue); // OP
      result = (int) vh.compareAndExchangeAcquire(this, field, newValue); // OP
      result = (int) vh.compareAndExchangeAcquire(this, field, newValue); // OP
      result = (int) vh.compareAndExchangeAcquire(this, field, newValue); // OP
      result = (int) vh.compareAndExchangeAcquire(this, field, newValue); // OP
      result = (int) vh.compareAndExchangeAcquire(this, field, newValue); // OP
      result = (int) vh.compareAndExchangeAcquire(this, field, newValue); // OP
      result = (int) vh.compareAndExchangeAcquire(this, field, newValue); // OP
      result = (int) vh.compareAndExchangeAcquire(this, field, newValue); // OP
      result = (int) vh.compareAndExchangeAcquire(this, field, newValue); // OP
      result = (int) vh.compareAndExchangeAcquire(this, field, newValue); // OP
      result = (int) vh.compareAndExchangeAcquire(this, field, newValue); // OP
    }
  }

  @Override
  public int innerIterations() {
      return 3000;
  }
}

class VarHandleCompareAndExchangeAcquireFieldStringBenchmark extends MicroBenchmark {
  static final String FIELD_VALUE = "field value";
  String field = null;
  VarHandle vh;

  VarHandleCompareAndExchangeAcquireFieldStringBenchmark() throws Throwable {
    vh = MethodHandles.lookup().findVarHandle(this.getClass(), "field", String.class);
  }

  @Override
  public void run() {
    String newValue = (field != null) ? null : FIELD_VALUE;
    String result;
    for (int pass = 0; pass < 100; ++pass) {
      result = (String) vh.compareAndExchangeAcquire(this, field, newValue); // OP
      result = (String) vh.compareAndExchangeAcquire(this, field, newValue); // OP
      result = (String) vh.compareAndExchangeAcquire(this, field, newValue); // OP
      result = (String) vh.compareAndExchangeAcquire(this, field, newValue); // OP
      result = (String) vh.compareAndExchangeAcquire(this, field, newValue); // OP
      result = (String) vh.compareAndExchangeAcquire(this, field, newValue); // OP
      result = (String) vh.compareAndExchangeAcquire(this, field, newValue); // OP
      result = (String) vh.compareAndExchangeAcquire(this, field, newValue); // OP
      result = (String) vh.compareAndExchangeAcquire(this, field, newValue); // OP
      result = (String) vh.compareAndExchangeAcquire(this, field, newValue); // OP
      result = (String) vh.compareAndExchangeAcquire(this, field, newValue); // OP
      result = (String) vh.compareAndExchangeAcquire(this, field, newValue); // OP
      result = (String) vh.compareAndExchangeAcquire(this, field, newValue); // OP
      result = (String) vh.compareAndExchangeAcquire(this, field, newValue); // OP
      result = (String) vh.compareAndExchangeAcquire(this, field, newValue); // OP
      result = (String) vh.compareAndExchangeAcquire(this, field, newValue); // OP
      result = (String) vh.compareAndExchangeAcquire(this, field, newValue); // OP
      result = (String) vh.compareAndExchangeAcquire(this, field, newValue); // OP
      result = (String) vh.compareAndExchangeAcquire(this, field, newValue); // OP
      result = (String) vh.compareAndExchangeAcquire(this, field, newValue); // OP
      result = (String) vh.compareAndExchangeAcquire(this, field, newValue); // OP
      result = (String) vh.compareAndExchangeAcquire(this, field, newValue); // OP
      result = (String) vh.compareAndExchangeAcquire(this, field, newValue); // OP
      result = (String) vh.compareAndExchangeAcquire(this, field, newValue); // OP
      result = (String) vh.compareAndExchangeAcquire(this, field, newValue); // OP
      result = (String) vh.compareAndExchangeAcquire(this, field, newValue); // OP
      result = (String) vh.compareAndExchangeAcquire(this, field, newValue); // OP
      result = (String) vh.compareAndExchangeAcquire(this, field, newValue); // OP
      result = (String) vh.compareAndExchangeAcquire(this, field, newValue); // OP
      result = (String) vh.compareAndExchangeAcquire(this, field, newValue); // OP
    }
  }

  @Override
  public int innerIterations() {
      return 3000;
  }
}

class VarHandleCompareAndExchangeReleaseStaticFieldIntBenchmark extends MicroBenchmark {
  static final int FIELD_VALUE = 42;
  static int field = FIELD_VALUE;

  VarHandle vh;

  VarHandleCompareAndExchangeReleaseStaticFieldIntBenchmark() throws Throwable {
    vh = MethodHandles.lookup().findStaticVarHandle(this.getClass(), "field", int.class);
  }

  @Override
  public void run() {
    int newValue = (field != 0) ? 0 : FIELD_VALUE;
    int result;
    for (int pass = 0; pass < 100; ++pass) {
      result = (int) vh.compareAndExchangeRelease(field, newValue); // OP
      result = (int) vh.compareAndExchangeRelease(field, newValue); // OP
      result = (int) vh.compareAndExchangeRelease(field, newValue); // OP
      result = (int) vh.compareAndExchangeRelease(field, newValue); // OP
      result = (int) vh.compareAndExchangeRelease(field, newValue); // OP
      result = (int) vh.compareAndExchangeRelease(field, newValue); // OP
      result = (int) vh.compareAndExchangeRelease(field, newValue); // OP
      result = (int) vh.compareAndExchangeRelease(field, newValue); // OP
      result = (int) vh.compareAndExchangeRelease(field, newValue); // OP
      result = (int) vh.compareAndExchangeRelease(field, newValue); // OP
      result = (int) vh.compareAndExchangeRelease(field, newValue); // OP
      result = (int) vh.compareAndExchangeRelease(field, newValue); // OP
      result = (int) vh.compareAndExchangeRelease(field, newValue); // OP
      result = (int) vh.compareAndExchangeRelease(field, newValue); // OP
      result = (int) vh.compareAndExchangeRelease(field, newValue); // OP
      result = (int) vh.compareAndExchangeRelease(field, newValue); // OP
      result = (int) vh.compareAndExchangeRelease(field, newValue); // OP
      result = (int) vh.compareAndExchangeRelease(field, newValue); // OP
      result = (int) vh.compareAndExchangeRelease(field, newValue); // OP
      result = (int) vh.compareAndExchangeRelease(field, newValue); // OP
      result = (int) vh.compareAndExchangeRelease(field, newValue); // OP
      result = (int) vh.compareAndExchangeRelease(field, newValue); // OP
      result = (int) vh.compareAndExchangeRelease(field, newValue); // OP
      result = (int) vh.compareAndExchangeRelease(field, newValue); // OP
      result = (int) vh.compareAndExchangeRelease(field, newValue); // OP
      result = (int) vh.compareAndExchangeRelease(field, newValue); // OP
      result = (int) vh.compareAndExchangeRelease(field, newValue); // OP
      result = (int) vh.compareAndExchangeRelease(field, newValue); // OP
      result = (int) vh.compareAndExchangeRelease(field, newValue); // OP
      result = (int) vh.compareAndExchangeRelease(field, newValue); // OP
    }
  }

  @Override
  public int innerIterations() {
      return 3000;
  }
}

class VarHandleCompareAndExchangeReleaseStaticFieldStringBenchmark extends MicroBenchmark {
  static final String FIELD_VALUE = "a string";
  static String field = null;

  VarHandle vh;

  VarHandleCompareAndExchangeReleaseStaticFieldStringBenchmark() throws Throwable {
    vh = MethodHandles.lookup().findStaticVarHandle(this.getClass(), "field", String.class);
  }

  @Override
  public void run() {
    String newValue = (field != null) ? null : FIELD_VALUE;
    String result;
    for (int pass = 0; pass < 100; ++pass) {
      result = (String) vh.compareAndExchangeRelease(field, newValue); // OP
      result = (String) vh.compareAndExchangeRelease(field, newValue); // OP
      result = (String) vh.compareAndExchangeRelease(field, newValue); // OP
      result = (String) vh.compareAndExchangeRelease(field, newValue); // OP
      result = (String) vh.compareAndExchangeRelease(field, newValue); // OP
      result = (String) vh.compareAndExchangeRelease(field, newValue); // OP
      result = (String) vh.compareAndExchangeRelease(field, newValue); // OP
      result = (String) vh.compareAndExchangeRelease(field, newValue); // OP
      result = (String) vh.compareAndExchangeRelease(field, newValue); // OP
      result = (String) vh.compareAndExchangeRelease(field, newValue); // OP
      result = (String) vh.compareAndExchangeRelease(field, newValue); // OP
      result = (String) vh.compareAndExchangeRelease(field, newValue); // OP
      result = (String) vh.compareAndExchangeRelease(field, newValue); // OP
      result = (String) vh.compareAndExchangeRelease(field, newValue); // OP
      result = (String) vh.compareAndExchangeRelease(field, newValue); // OP
      result = (String) vh.compareAndExchangeRelease(field, newValue); // OP
      result = (String) vh.compareAndExchangeRelease(field, newValue); // OP
      result = (String) vh.compareAndExchangeRelease(field, newValue); // OP
      result = (String) vh.compareAndExchangeRelease(field, newValue); // OP
      result = (String) vh.compareAndExchangeRelease(field, newValue); // OP
      result = (String) vh.compareAndExchangeRelease(field, newValue); // OP
      result = (String) vh.compareAndExchangeRelease(field, newValue); // OP
      result = (String) vh.compareAndExchangeRelease(field, newValue); // OP
      result = (String) vh.compareAndExchangeRelease(field, newValue); // OP
      result = (String) vh.compareAndExchangeRelease(field, newValue); // OP
      result = (String) vh.compareAndExchangeRelease(field, newValue); // OP
      result = (String) vh.compareAndExchangeRelease(field, newValue); // OP
      result = (String) vh.compareAndExchangeRelease(field, newValue); // OP
      result = (String) vh.compareAndExchangeRelease(field, newValue); // OP
      result = (String) vh.compareAndExchangeRelease(field, newValue); // OP
    }
  }

  @Override
  public int innerIterations() {
      return 3000;
  }
}

class VarHandleCompareAndExchangeReleaseFieldIntBenchmark extends MicroBenchmark {
  static final int FIELD_VALUE = 42;

  int field = FIELD_VALUE;
  VarHandle vh;

  VarHandleCompareAndExchangeReleaseFieldIntBenchmark() throws Throwable {
    vh = MethodHandles.lookup().findVarHandle(this.getClass(), "field", int.class);
  }

  @Override
  public void run() {
    int newValue = (field != 0) ? 0 : FIELD_VALUE;
    int result;
    for (int pass = 0; pass < 100; ++pass) {
      result = (int) vh.compareAndExchangeRelease(this, field, newValue); // OP
      result = (int) vh.compareAndExchangeRelease(this, field, newValue); // OP
      result = (int) vh.compareAndExchangeRelease(this, field, newValue); // OP
      result = (int) vh.compareAndExchangeRelease(this, field, newValue); // OP
      result = (int) vh.compareAndExchangeRelease(this, field, newValue); // OP
      result = (int) vh.compareAndExchangeRelease(this, field, newValue); // OP
      result = (int) vh.compareAndExchangeRelease(this, field, newValue); // OP
      result = (int) vh.compareAndExchangeRelease(this, field, newValue); // OP
      result = (int) vh.compareAndExchangeRelease(this, field, newValue); // OP
      result = (int) vh.compareAndExchangeRelease(this, field, newValue); // OP
      result = (int) vh.compareAndExchangeRelease(this, field, newValue); // OP
      result = (int) vh.compareAndExchangeRelease(this, field, newValue); // OP
      result = (int) vh.compareAndExchangeRelease(this, field, newValue); // OP
      result = (int) vh.compareAndExchangeRelease(this, field, newValue); // OP
      result = (int) vh.compareAndExchangeRelease(this, field, newValue); // OP
      result = (int) vh.compareAndExchangeRelease(this, field, newValue); // OP
      result = (int) vh.compareAndExchangeRelease(this, field, newValue); // OP
      result = (int) vh.compareAndExchangeRelease(this, field, newValue); // OP
      result = (int) vh.compareAndExchangeRelease(this, field, newValue); // OP
      result = (int) vh.compareAndExchangeRelease(this, field, newValue); // OP
      result = (int) vh.compareAndExchangeRelease(this, field, newValue); // OP
      result = (int) vh.compareAndExchangeRelease(this, field, newValue); // OP
      result = (int) vh.compareAndExchangeRelease(this, field, newValue); // OP
      result = (int) vh.compareAndExchangeRelease(this, field, newValue); // OP
      result = (int) vh.compareAndExchangeRelease(this, field, newValue); // OP
      result = (int) vh.compareAndExchangeRelease(this, field, newValue); // OP
      result = (int) vh.compareAndExchangeRelease(this, field, newValue); // OP
      result = (int) vh.compareAndExchangeRelease(this, field, newValue); // OP
      result = (int) vh.compareAndExchangeRelease(this, field, newValue); // OP
      result = (int) vh.compareAndExchangeRelease(this, field, newValue); // OP
    }
  }

  @Override
  public int innerIterations() {
      return 3000;
  }
}

class VarHandleCompareAndExchangeReleaseFieldStringBenchmark extends MicroBenchmark {
  static final String FIELD_VALUE = "field value";
  String field = null;
  VarHandle vh;

  VarHandleCompareAndExchangeReleaseFieldStringBenchmark() throws Throwable {
    vh = MethodHandles.lookup().findVarHandle(this.getClass(), "field", String.class);
  }

  @Override
  public void run() {
    String newValue = (field != null) ? null : FIELD_VALUE;
    String result;
    for (int pass = 0; pass < 100; ++pass) {
      result = (String) vh.compareAndExchangeRelease(this, field, newValue); // OP
      result = (String) vh.compareAndExchangeRelease(this, field, newValue); // OP
      result = (String) vh.compareAndExchangeRelease(this, field, newValue); // OP
      result = (String) vh.compareAndExchangeRelease(this, field, newValue); // OP
      result = (String) vh.compareAndExchangeRelease(this, field, newValue); // OP
      result = (String) vh.compareAndExchangeRelease(this, field, newValue); // OP
      result = (String) vh.compareAndExchangeRelease(this, field, newValue); // OP
      result = (String) vh.compareAndExchangeRelease(this, field, newValue); // OP
      result = (String) vh.compareAndExchangeRelease(this, field, newValue); // OP
      result = (String) vh.compareAndExchangeRelease(this, field, newValue); // OP
      result = (String) vh.compareAndExchangeRelease(this, field, newValue); // OP
      result = (String) vh.compareAndExchangeRelease(this, field, newValue); // OP
      result = (String) vh.compareAndExchangeRelease(this, field, newValue); // OP
      result = (String) vh.compareAndExchangeRelease(this, field, newValue); // OP
      result = (String) vh.compareAndExchangeRelease(this, field, newValue); // OP
      result = (String) vh.compareAndExchangeRelease(this, field, newValue); // OP
      result = (String) vh.compareAndExchangeRelease(this, field, newValue); // OP
      result = (String) vh.compareAndExchangeRelease(this, field, newValue); // OP
      result = (String) vh.compareAndExchangeRelease(this, field, newValue); // OP
      result = (String) vh.compareAndExchangeRelease(this, field, newValue); // OP
      result = (String) vh.compareAndExchangeRelease(this, field, newValue); // OP
      result = (String) vh.compareAndExchangeRelease(this, field, newValue); // OP
      result = (String) vh.compareAndExchangeRelease(this, field, newValue); // OP
      result = (String) vh.compareAndExchangeRelease(this, field, newValue); // OP
      result = (String) vh.compareAndExchangeRelease(this, field, newValue); // OP
      result = (String) vh.compareAndExchangeRelease(this, field, newValue); // OP
      result = (String) vh.compareAndExchangeRelease(this, field, newValue); // OP
      result = (String) vh.compareAndExchangeRelease(this, field, newValue); // OP
      result = (String) vh.compareAndExchangeRelease(this, field, newValue); // OP
      result = (String) vh.compareAndExchangeRelease(this, field, newValue); // OP
    }
  }

  @Override
  public int innerIterations() {
      return 3000;
  }
}

class VarHandleGetArrayElementIntBenchmark extends MicroBenchmark {
  static final int ELEMENT_VALUE = 42;

  int[] array = { ELEMENT_VALUE };
  VarHandle vh;

  VarHandleGetArrayElementIntBenchmark() throws Throwable {
    vh = MethodHandles.arrayElementVarHandle(int[].class);
  }

  @Override
  public void setup() {
    int value = (int) vh.get(array, 0);
    if (value != ELEMENT_VALUE) {
      throw new RuntimeException("vh.get() returned " + value);
    }
  }

  @Override
  public void run() {
    int[] a = array;
    int index = 0;
    int x;
    for (int pass = 0; pass < 100; ++pass) {
      x = (int) vh.get(a, index); // OP
      x = (int) vh.get(a, index); // OP
      x = (int) vh.get(a, index); // OP
      x = (int) vh.get(a, index); // OP
      x = (int) vh.get(a, index); // OP
      x = (int) vh.get(a, index); // OP
      x = (int) vh.get(a, index); // OP
      x = (int) vh.get(a, index); // OP
      x = (int) vh.get(a, index); // OP
      x = (int) vh.get(a, index); // OP
      x = (int) vh.get(a, index); // OP
      x = (int) vh.get(a, index); // OP
      x = (int) vh.get(a, index); // OP
      x = (int) vh.get(a, index); // OP
      x = (int) vh.get(a, index); // OP
      x = (int) vh.get(a, index); // OP
      x = (int) vh.get(a, index); // OP
      x = (int) vh.get(a, index); // OP
      x = (int) vh.get(a, index); // OP
      x = (int) vh.get(a, index); // OP
      x = (int) vh.get(a, index); // OP
      x = (int) vh.get(a, index); // OP
      x = (int) vh.get(a, index); // OP
      x = (int) vh.get(a, index); // OP
      x = (int) vh.get(a, index); // OP
      x = (int) vh.get(a, index); // OP
      x = (int) vh.get(a, index); // OP
      x = (int) vh.get(a, index); // OP
      x = (int) vh.get(a, index); // OP
      x = (int) vh.get(a, index); // OP
    }
  }

  @Override
  public int innerIterations() {
      return 3000;
  }
}

class VarHandleGetArrayElementStringBenchmark extends MicroBenchmark {
  static final String ELEMENT_VALUE = "element value";

  String[] array = { ELEMENT_VALUE };
  VarHandle vh;

  VarHandleGetArrayElementStringBenchmark() throws Throwable {
    vh = MethodHandles.arrayElementVarHandle(String[].class);
  }

  @Override
  public void setup() {
    String value = (String) vh.get(array, 0);
    if (!value.equals(ELEMENT_VALUE)) {
      throw new RuntimeException("vh.get() returned " + value);
    }
  }

  @Override
  public void run() {
    String[] a = array;
    int index = 0;
    String x;
    for (int pass = 0; pass < 100; ++pass) {
      x = (String) vh.get(a, index); // OP
      x = (String) vh.get(a, index); // OP
      x = (String) vh.get(a, index); // OP
      x = (String) vh.get(a, index); // OP
      x = (String) vh.get(a, index); // OP
      x = (String) vh.get(a, index); // OP
      x = (String) vh.get(a, index); // OP
      x = (String) vh.get(a, index); // OP
      x = (String) vh.get(a, index); // OP
      x = (String) vh.get(a, index); // OP
      x = (String) vh.get(a, index); // OP
      x = (String) vh.get(a, index); // OP
      x = (String) vh.get(a, index); // OP
      x = (String) vh.get(a, index); // OP
      x = (String) vh.get(a, index); // OP
      x = (String) vh.get(a, index); // OP
      x = (String) vh.get(a, index); // OP
      x = (String) vh.get(a, index); // OP
      x = (String) vh.get(a, index); // OP
      x = (String) vh.get(a, index); // OP
      x = (String) vh.get(a, index); // OP
      x = (String) vh.get(a, index); // OP
      x = (String) vh.get(a, index); // OP
      x = (String) vh.get(a, index); // OP
      x = (String) vh.get(a, index); // OP
      x = (String) vh.get(a, index); // OP
      x = (String) vh.get(a, index); // OP
      x = (String) vh.get(a, index); // OP
      x = (String) vh.get(a, index); // OP
      x = (String) vh.get(a, index); // OP
    }
  }

  @Override
  public int innerIterations() {
      return 3000;
  }
}

class VarHandleSetArrayElementIntBenchmark extends MicroBenchmark {
  static final int ELEMENT_VALUE = 42;
  int[] array = { ~ELEMENT_VALUE };
  VarHandle vh;

  VarHandleSetArrayElementIntBenchmark() throws Throwable {
    vh = MethodHandles.arrayElementVarHandle(int[].class);
  }

  @Override
  public void teardown() {
    if (array[0] != ELEMENT_VALUE) {
      throw new RuntimeException("array[0] has unexpected value: " + array[0]);
    }
  }

  @Override
  public void run() {
    int[] a = array;
    int index = 0;
    for (int pass = 0; pass < 100; ++pass) {
      vh.set(a, index, ELEMENT_VALUE); // OP
      vh.set(a, index, ELEMENT_VALUE); // OP
      vh.set(a, index, ELEMENT_VALUE); // OP
      vh.set(a, index, ELEMENT_VALUE); // OP
      vh.set(a, index, ELEMENT_VALUE); // OP
      vh.set(a, index, ELEMENT_VALUE); // OP
      vh.set(a, index, ELEMENT_VALUE); // OP
      vh.set(a, index, ELEMENT_VALUE); // OP
      vh.set(a, index, ELEMENT_VALUE); // OP
      vh.set(a, index, ELEMENT_VALUE); // OP
      vh.set(a, index, ELEMENT_VALUE); // OP
      vh.set(a, index, ELEMENT_VALUE); // OP
      vh.set(a, index, ELEMENT_VALUE); // OP
      vh.set(a, index, ELEMENT_VALUE); // OP
      vh.set(a, index, ELEMENT_VALUE); // OP
      vh.set(a, index, ELEMENT_VALUE); // OP
      vh.set(a, index, ELEMENT_VALUE); // OP
      vh.set(a, index, ELEMENT_VALUE); // OP
      vh.set(a, index, ELEMENT_VALUE); // OP
      vh.set(a, index, ELEMENT_VALUE); // OP
      vh.set(a, index, ELEMENT_VALUE); // OP
      vh.set(a, index, ELEMENT_VALUE); // OP
      vh.set(a, index, ELEMENT_VALUE); // OP
      vh.set(a, index, ELEMENT_VALUE); // OP
      vh.set(a, index, ELEMENT_VALUE); // OP
      vh.set(a, index, ELEMENT_VALUE); // OP
      vh.set(a, index, ELEMENT_VALUE); // OP
      vh.set(a, index, ELEMENT_VALUE); // OP
      vh.set(a, index, ELEMENT_VALUE); // OP
      vh.set(a, index, ELEMENT_VALUE); // OP
    }
  }

  @Override
  public int innerIterations() {
      return 3000;
  }
}

class VarHandleSetArrayElementStringBenchmark extends MicroBenchmark {
  static final String ELEMENT_VALUE = "field value";
  String[] array = { null };
  VarHandle vh;

  VarHandleSetArrayElementStringBenchmark() throws Throwable {
    vh = MethodHandles.arrayElementVarHandle(String[].class);
  }

  @Override
  public void teardown() {
    if (!ELEMENT_VALUE.equals(array[0])) {
      throw new RuntimeException("array[0] has unexpected value: " + array[0]);
    }
  }

  @Override
  public void run() {
    String[] a = array;
    int index = 0;
    for (int pass = 0; pass < 100; ++pass) {
      vh.set(a, index, ELEMENT_VALUE); // OP
      vh.set(a, index, ELEMENT_VALUE); // OP
      vh.set(a, index, ELEMENT_VALUE); // OP
      vh.set(a, index, ELEMENT_VALUE); // OP
      vh.set(a, index, ELEMENT_VALUE); // OP
      vh.set(a, index, ELEMENT_VALUE); // OP
      vh.set(a, index, ELEMENT_VALUE); // OP
      vh.set(a, index, ELEMENT_VALUE); // OP
      vh.set(a, index, ELEMENT_VALUE); // OP
      vh.set(a, index, ELEMENT_VALUE); // OP
      vh.set(a, index, ELEMENT_VALUE); // OP
      vh.set(a, index, ELEMENT_VALUE); // OP
      vh.set(a, index, ELEMENT_VALUE); // OP
      vh.set(a, index, ELEMENT_VALUE); // OP
      vh.set(a, index, ELEMENT_VALUE); // OP
      vh.set(a, index, ELEMENT_VALUE); // OP
      vh.set(a, index, ELEMENT_VALUE); // OP
      vh.set(a, index, ELEMENT_VALUE); // OP
      vh.set(a, index, ELEMENT_VALUE); // OP
      vh.set(a, index, ELEMENT_VALUE); // OP
      vh.set(a, index, ELEMENT_VALUE); // OP
      vh.set(a, index, ELEMENT_VALUE); // OP
      vh.set(a, index, ELEMENT_VALUE); // OP
      vh.set(a, index, ELEMENT_VALUE); // OP
      vh.set(a, index, ELEMENT_VALUE); // OP
      vh.set(a, index, ELEMENT_VALUE); // OP
      vh.set(a, index, ELEMENT_VALUE); // OP
      vh.set(a, index, ELEMENT_VALUE); // OP
      vh.set(a, index, ELEMENT_VALUE); // OP
      vh.set(a, index, ELEMENT_VALUE); // OP
    }
  }

  @Override
  public int innerIterations() {
      return 3000;
  }
}

class VarHandleGetByteArrayViewIntBenchmark extends MicroBenchmark {
  static final int ELEMENT_VALUE = 42;

  byte[] array = {
      (byte) ELEMENT_VALUE,
      (byte) (ELEMENT_VALUE >> 8),
      (byte) (ELEMENT_VALUE >> 16),
      (byte) (ELEMENT_VALUE >> 24)
  };
  VarHandle vh;

  VarHandleGetByteArrayViewIntBenchmark() throws Throwable {
    vh = MethodHandles.byteArrayViewVarHandle(int[].class, ByteOrder.LITTLE_ENDIAN);
  }

  @Override
  public void setup() {
    int value = (int) vh.get(array, 0);
    if (value != ELEMENT_VALUE) {
      throw new RuntimeException("vh.get() returned " + value);
    }
  }

  @Override
  public void run() {
    byte[] a = array;
    int index = 0;
    int x;
    for (int pass = 0; pass < 100; ++pass) {
      x = (int) vh.get(a, index); // OP
      x = (int) vh.get(a, index); // OP
      x = (int) vh.get(a, index); // OP
      x = (int) vh.get(a, index); // OP
      x = (int) vh.get(a, index); // OP
      x = (int) vh.get(a, index); // OP
      x = (int) vh.get(a, index); // OP
      x = (int) vh.get(a, index); // OP
      x = (int) vh.get(a, index); // OP
      x = (int) vh.get(a, index); // OP
      x = (int) vh.get(a, index); // OP
      x = (int) vh.get(a, index); // OP
      x = (int) vh.get(a, index); // OP
      x = (int) vh.get(a, index); // OP
      x = (int) vh.get(a, index); // OP
      x = (int) vh.get(a, index); // OP
      x = (int) vh.get(a, index); // OP
      x = (int) vh.get(a, index); // OP
      x = (int) vh.get(a, index); // OP
      x = (int) vh.get(a, index); // OP
      x = (int) vh.get(a, index); // OP
      x = (int) vh.get(a, index); // OP
      x = (int) vh.get(a, index); // OP
      x = (int) vh.get(a, index); // OP
      x = (int) vh.get(a, index); // OP
      x = (int) vh.get(a, index); // OP
      x = (int) vh.get(a, index); // OP
      x = (int) vh.get(a, index); // OP
      x = (int) vh.get(a, index); // OP
      x = (int) vh.get(a, index); // OP
    }
  }

  @Override
  public int innerIterations() {
      return 3000;
  }
}

class VarHandleSetByteArrayViewIntBenchmark extends MicroBenchmark {
  static final int ELEMENT_VALUE = 42;
  byte[] array = { (byte) ~ELEMENT_VALUE, (byte) -1, (byte) -1, (byte) -1 };
  VarHandle vh;

  VarHandleSetByteArrayViewIntBenchmark() throws Throwable {
    vh = MethodHandles.byteArrayViewVarHandle(int[].class, ByteOrder.LITTLE_ENDIAN);
  }

  @Override
  public void teardown() {
    if ((array[0] != (byte) ELEMENT_VALUE) ||
        (array[1] != (byte) (ELEMENT_VALUE >> 8)) ||
        (array[2] != (byte) (ELEMENT_VALUE >> 16)) ||
        (array[3] != (byte) (ELEMENT_VALUE >> 24))) {
      throw new RuntimeException("array[] has unexpected values: " +
          array[0] + " " + array[1] + " " + array[2] + " " + array[3]);
    }
  }

  @Override
  public void run() {
    byte[] a = array;
    int index = 0;
    for (int pass = 0; pass < 100; ++pass) {
      vh.set(a, index, ELEMENT_VALUE); // OP
      vh.set(a, index, ELEMENT_VALUE); // OP
      vh.set(a, index, ELEMENT_VALUE); // OP
      vh.set(a, index, ELEMENT_VALUE); // OP
      vh.set(a, index, ELEMENT_VALUE); // OP
      vh.set(a, index, ELEMENT_VALUE); // OP
      vh.set(a, index, ELEMENT_VALUE); // OP
      vh.set(a, index, ELEMENT_VALUE); // OP
      vh.set(a, index, ELEMENT_VALUE); // OP
      vh.set(a, index, ELEMENT_VALUE); // OP
      vh.set(a, index, ELEMENT_VALUE); // OP
      vh.set(a, index, ELEMENT_VALUE); // OP
      vh.set(a, index, ELEMENT_VALUE); // OP
      vh.set(a, index, ELEMENT_VALUE); // OP
      vh.set(a, index, ELEMENT_VALUE); // OP
      vh.set(a, index, ELEMENT_VALUE); // OP
      vh.set(a, index, ELEMENT_VALUE); // OP
      vh.set(a, index, ELEMENT_VALUE); // OP
      vh.set(a, index, ELEMENT_VALUE); // OP
      vh.set(a, index, ELEMENT_VALUE); // OP
      vh.set(a, index, ELEMENT_VALUE); // OP
      vh.set(a, index, ELEMENT_VALUE); // OP
      vh.set(a, index, ELEMENT_VALUE); // OP
      vh.set(a, index, ELEMENT_VALUE); // OP
      vh.set(a, index, ELEMENT_VALUE); // OP
      vh.set(a, index, ELEMENT_VALUE); // OP
      vh.set(a, index, ELEMENT_VALUE); // OP
      vh.set(a, index, ELEMENT_VALUE); // OP
      vh.set(a, index, ELEMENT_VALUE); // OP
      vh.set(a, index, ELEMENT_VALUE); // OP
    }
  }

  @Override
  public int innerIterations() {
      return 3000;
  }
}

class VarHandleGetByteArrayViewBigEndianIntBenchmark extends MicroBenchmark {
  static final int ELEMENT_VALUE = 42;

  byte[] array = {
      (byte) (ELEMENT_VALUE >> 24),
      (byte) (ELEMENT_VALUE >> 16),
      (byte) (ELEMENT_VALUE >> 8),
      (byte) ELEMENT_VALUE
  };
  VarHandle vh;

  VarHandleGetByteArrayViewBigEndianIntBenchmark() throws Throwable {
    vh = MethodHandles.byteArrayViewVarHandle(int[].class, ByteOrder.BIG_ENDIAN);
  }

  @Override
  public void setup() {
    int value = (int) vh.get(array, 0);
    if (value != ELEMENT_VALUE) {
      throw new RuntimeException("vh.get() returned " + value);
    }
  }

  @Override
  public void run() {
    byte[] a = array;
    int index = 0;
    int x;
    for (int pass = 0; pass < 100; ++pass) {
      x = (int) vh.get(a, index); // OP
      x = (int) vh.get(a, index); // OP
      x = (int) vh.get(a, index); // OP
      x = (int) vh.get(a, index); // OP
      x = (int) vh.get(a, index); // OP
      x = (int) vh.get(a, index); // OP
      x = (int) vh.get(a, index); // OP
      x = (int) vh.get(a, index); // OP
      x = (int) vh.get(a, index); // OP
      x = (int) vh.get(a, index); // OP
      x = (int) vh.get(a, index); // OP
      x = (int) vh.get(a, index); // OP
      x = (int) vh.get(a, index); // OP
      x = (int) vh.get(a, index); // OP
      x = (int) vh.get(a, index); // OP
      x = (int) vh.get(a, index); // OP
      x = (int) vh.get(a, index); // OP
      x = (int) vh.get(a, index); // OP
      x = (int) vh.get(a, index); // OP
      x = (int) vh.get(a, index); // OP
      x = (int) vh.get(a, index); // OP
      x = (int) vh.get(a, index); // OP
      x = (int) vh.get(a, index); // OP
      x = (int) vh.get(a, index); // OP
      x = (int) vh.get(a, index); // OP
      x = (int) vh.get(a, index); // OP
      x = (int) vh.get(a, index); // OP
      x = (int) vh.get(a, index); // OP
      x = (int) vh.get(a, index); // OP
      x = (int) vh.get(a, index); // OP
    }
  }

  @Override
  public int innerIterations() {
      return 3000;
  }
}

class VarHandleSetByteArrayViewBigEndianIntBenchmark extends MicroBenchmark {
  static final int ELEMENT_VALUE = 42;
  byte[] array = { (byte) -1, (byte) -1, (byte) -1 , (byte) ~ELEMENT_VALUE };
  VarHandle vh;

  VarHandleSetByteArrayViewBigEndianIntBenchmark() throws Throwable {
    vh = MethodHandles.byteArrayViewVarHandle(int[].class, ByteOrder.BIG_ENDIAN);
  }

  @Override
  public void teardown() {
    if ((array[0] != (byte) (ELEMENT_VALUE >> 24)) ||
        (array[1] != (byte) (ELEMENT_VALUE >> 16)) ||
        (array[2] != (byte) (ELEMENT_VALUE >> 8)) ||
        (array[3] != (byte) ELEMENT_VALUE)) {
      throw new RuntimeException("array[] has unexpected values: " +
          array[0] + " " + array[1] + " " + array[2] + " " + array[3]);
    }
  }

  @Override
  public void run() {
    byte[] a = array;
    int index = 0;
    for (int pass = 0; pass < 100; ++pass) {
      vh.set(a, index, ELEMENT_VALUE); // OP
      vh.set(a, index, ELEMENT_VALUE); // OP
      vh.set(a, index, ELEMENT_VALUE); // OP
      vh.set(a, index, ELEMENT_VALUE); // OP
      vh.set(a, index, ELEMENT_VALUE); // OP
      vh.set(a, index, ELEMENT_VALUE); // OP
      vh.set(a, index, ELEMENT_VALUE); // OP
      vh.set(a, index, ELEMENT_VALUE); // OP
      vh.set(a, index, ELEMENT_VALUE); // OP
      vh.set(a, index, ELEMENT_VALUE); // OP
      vh.set(a, index, ELEMENT_VALUE); // OP
      vh.set(a, index, ELEMENT_VALUE); // OP
      vh.set(a, index, ELEMENT_VALUE); // OP
      vh.set(a, index, ELEMENT_VALUE); // OP
      vh.set(a, index, ELEMENT_VALUE); // OP
      vh.set(a, index, ELEMENT_VALUE); // OP
      vh.set(a, index, ELEMENT_VALUE); // OP
      vh.set(a, index, ELEMENT_VALUE); // OP
      vh.set(a, index, ELEMENT_VALUE); // OP
      vh.set(a, index, ELEMENT_VALUE); // OP
      vh.set(a, index, ELEMENT_VALUE); // OP
      vh.set(a, index, ELEMENT_VALUE); // OP
      vh.set(a, index, ELEMENT_VALUE); // OP
      vh.set(a, index, ELEMENT_VALUE); // OP
      vh.set(a, index, ELEMENT_VALUE); // OP
      vh.set(a, index, ELEMENT_VALUE); // OP
      vh.set(a, index, ELEMENT_VALUE); // OP
      vh.set(a, index, ELEMENT_VALUE); // OP
      vh.set(a, index, ELEMENT_VALUE); // OP
      vh.set(a, index, ELEMENT_VALUE); // OP
    }
  }

  @Override
  public int innerIterations() {
      return 3000;
  }
}

class ReflectGetStaticFieldIntBenchmark extends MicroBenchmark {
  Field field;
  static int value;

  ReflectGetStaticFieldIntBenchmark() throws Throwable {
    field = this.getClass().getDeclaredField("value");
  }

  @Override
  public void run() throws Throwable {
    int x;
    for (int pass = 0; pass < 100; ++pass) {
      x = field.getInt(null); // OP
      x = field.getInt(null); // OP
      x = field.getInt(null); // OP
      x = field.getInt(null); // OP
      x = field.getInt(null); // OP
      x = field.getInt(null); // OP
      x = field.getInt(null); // OP
      x = field.getInt(null); // OP
      x = field.getInt(null); // OP
      x = field.getInt(null); // OP
      x = field.getInt(null); // OP
      x = field.getInt(null); // OP
      x = field.getInt(null); // OP
      x = field.getInt(null); // OP
      x = field.getInt(null); // OP
      x = field.getInt(null); // OP
      x = field.getInt(null); // OP
      x = field.getInt(null); // OP
      x = field.getInt(null); // OP
      x = field.getInt(null); // OP
      x = field.getInt(null); // OP
      x = field.getInt(null); // OP
      x = field.getInt(null); // OP
      x = field.getInt(null); // OP
      x = field.getInt(null); // OP
      x = field.getInt(null); // OP
      x = field.getInt(null); // OP
      x = field.getInt(null); // OP
      x = field.getInt(null); // OP
      x = field.getInt(null); // OP
    }
  }

  @Override
  public int innerIterations() {
      return 3000;
  }
}

class ReflectGetStaticFieldStringBenchmark extends MicroBenchmark {
  Field field;
  static String value;

  ReflectGetStaticFieldStringBenchmark() throws Throwable {
    field = this.getClass().getDeclaredField("value");
  }

  @Override
  public void run() throws Throwable {
    String x;
    for (int pass = 0; pass < 100; ++pass) {
      x = (String) field.get(null); // OP
      x = (String) field.get(null); // OP
      x = (String) field.get(null); // OP
      x = (String) field.get(null); // OP
      x = (String) field.get(null); // OP
      x = (String) field.get(null); // OP
      x = (String) field.get(null); // OP
      x = (String) field.get(null); // OP
      x = (String) field.get(null); // OP
      x = (String) field.get(null); // OP
      x = (String) field.get(null); // OP
      x = (String) field.get(null); // OP
      x = (String) field.get(null); // OP
      x = (String) field.get(null); // OP
      x = (String) field.get(null); // OP
      x = (String) field.get(null); // OP
      x = (String) field.get(null); // OP
      x = (String) field.get(null); // OP
      x = (String) field.get(null); // OP
      x = (String) field.get(null); // OP
      x = (String) field.get(null); // OP
      x = (String) field.get(null); // OP
      x = (String) field.get(null); // OP
      x = (String) field.get(null); // OP
      x = (String) field.get(null); // OP
      x = (String) field.get(null); // OP
      x = (String) field.get(null); // OP
      x = (String) field.get(null); // OP
      x = (String) field.get(null); // OP
      x = (String) field.get(null); // OP
    }
  }

  @Override
  public int innerIterations() {
      return 3000;
  }
}

class ReflectGetFieldIntBenchmark extends MicroBenchmark {
  Field field;
  int value;

  ReflectGetFieldIntBenchmark() throws Throwable {
    field = this.getClass().getDeclaredField("value");
  }

  @Override
  public void run() throws Throwable {
    int x;
    for (int pass = 0; pass < 100; ++pass) {
      x = field.getInt(this); // OP
      x = field.getInt(this); // OP
      x = field.getInt(this); // OP
      x = field.getInt(this); // OP
      x = field.getInt(this); // OP
      x = field.getInt(this); // OP
      x = field.getInt(this); // OP
      x = field.getInt(this); // OP
      x = field.getInt(this); // OP
      x = field.getInt(this); // OP
      x = field.getInt(this); // OP
      x = field.getInt(this); // OP
      x = field.getInt(this); // OP
      x = field.getInt(this); // OP
      x = field.getInt(this); // OP
      x = field.getInt(this); // OP
      x = field.getInt(this); // OP
      x = field.getInt(this); // OP
      x = field.getInt(this); // OP
      x = field.getInt(this); // OP
      x = field.getInt(this); // OP
      x = field.getInt(this); // OP
      x = field.getInt(this); // OP
      x = field.getInt(this); // OP
      x = field.getInt(this); // OP
      x = field.getInt(this); // OP
      x = field.getInt(this); // OP
      x = field.getInt(this); // OP
      x = field.getInt(this); // OP
      x = field.getInt(this); // OP
    }
  }

  @Override
  public int innerIterations() {
      return 3000;
  }
}

class ReflectGetFieldStringBenchmark extends MicroBenchmark {
  Field field;
  String value;

  ReflectGetFieldStringBenchmark() throws Throwable {
    field = this.getClass().getDeclaredField("value");
  }

  @Override
  public void run() throws Throwable {
    String x;
    for (int pass = 0; pass < 100; ++pass) {
      x = (String) field.get(this); // OP
      x = (String) field.get(this); // OP
      x = (String) field.get(this); // OP
      x = (String) field.get(this); // OP
      x = (String) field.get(this); // OP
      x = (String) field.get(this); // OP
      x = (String) field.get(this); // OP
      x = (String) field.get(this); // OP
      x = (String) field.get(this); // OP
      x = (String) field.get(this); // OP
      x = (String) field.get(this); // OP
      x = (String) field.get(this); // OP
      x = (String) field.get(this); // OP
      x = (String) field.get(this); // OP
      x = (String) field.get(this); // OP
      x = (String) field.get(this); // OP
      x = (String) field.get(this); // OP
      x = (String) field.get(this); // OP
      x = (String) field.get(this); // OP
      x = (String) field.get(this); // OP
      x = (String) field.get(this); // OP
      x = (String) field.get(this); // OP
      x = (String) field.get(this); // OP
      x = (String) field.get(this); // OP
      x = (String) field.get(this); // OP
      x = (String) field.get(this); // OP
      x = (String) field.get(this); // OP
      x = (String) field.get(this); // OP
      x = (String) field.get(this); // OP
      x = (String) field.get(this); // OP
    }
  }

  @Override
  public int innerIterations() {
      return 3000;
  }
}

class ReflectSetStaticFieldIntBenchmark extends MicroBenchmark {
  Field field;
  static int value;

  ReflectSetStaticFieldIntBenchmark() throws Throwable {
    field = this.getClass().getDeclaredField("value");
  }

  @Override
  public void run() throws Throwable {
    for (int pass = 0; pass < 100; ++pass) {
      field.setInt(null, 42); // OP
      field.setInt(null, 42); // OP
      field.setInt(null, 42); // OP
      field.setInt(null, 42); // OP
      field.setInt(null, 42); // OP
      field.setInt(null, 42); // OP
      field.setInt(null, 42); // OP
      field.setInt(null, 42); // OP
      field.setInt(null, 42); // OP
      field.setInt(null, 42); // OP
      field.setInt(null, 42); // OP
      field.setInt(null, 42); // OP
      field.setInt(null, 42); // OP
      field.setInt(null, 42); // OP
      field.setInt(null, 42); // OP
      field.setInt(null, 42); // OP
      field.setInt(null, 42); // OP
      field.setInt(null, 42); // OP
      field.setInt(null, 42); // OP
      field.setInt(null, 42); // OP
      field.setInt(null, 42); // OP
      field.setInt(null, 42); // OP
      field.setInt(null, 42); // OP
      field.setInt(null, 42); // OP
      field.setInt(null, 42); // OP
      field.setInt(null, 42); // OP
      field.setInt(null, 42); // OP
      field.setInt(null, 42); // OP
      field.setInt(null, 42); // OP
      field.setInt(null, 42); // OP
    }
  }

  @Override
  public int innerIterations() {
      return 3000;
  }
}

class ReflectSetStaticFieldStringBenchmark extends MicroBenchmark {
  Field field;
  static String value;

  ReflectSetStaticFieldStringBenchmark() throws Throwable {
    field = this.getClass().getDeclaredField("value");
  }

  @Override
  public void run() throws Throwable {
    for (int pass = 0; pass < 100; ++pass) {
      field.set(null, "Peace and beans"); // OP
      field.set(null, "Peace and beans"); // OP
      field.set(null, "Peace and beans"); // OP
      field.set(null, "Peace and beans"); // OP
      field.set(null, "Peace and beans"); // OP
      field.set(null, "Peace and beans"); // OP
      field.set(null, "Peace and beans"); // OP
      field.set(null, "Peace and beans"); // OP
      field.set(null, "Peace and beans"); // OP
      field.set(null, "Peace and beans"); // OP
      field.set(null, "Peace and beans"); // OP
      field.set(null, "Peace and beans"); // OP
      field.set(null, "Peace and beans"); // OP
      field.set(null, "Peace and beans"); // OP
      field.set(null, "Peace and beans"); // OP
      field.set(null, "Peace and beans"); // OP
      field.set(null, "Peace and beans"); // OP
      field.set(null, "Peace and beans"); // OP
      field.set(null, "Peace and beans"); // OP
      field.set(null, "Peace and beans"); // OP
      field.set(null, "Peace and beans"); // OP
      field.set(null, "Peace and beans"); // OP
      field.set(null, "Peace and beans"); // OP
      field.set(null, "Peace and beans"); // OP
      field.set(null, "Peace and beans"); // OP
      field.set(null, "Peace and beans"); // OP
      field.set(null, "Peace and beans"); // OP
      field.set(null, "Peace and beans"); // OP
      field.set(null, "Peace and beans"); // OP
      field.set(null, "Peace and beans"); // OP
    }
  }

  @Override
  public int innerIterations() {
      return 3000;
  }
}

class ReflectSetFieldIntBenchmark extends MicroBenchmark {
  Field field;
  int value;

  ReflectSetFieldIntBenchmark() throws Throwable {
    field = this.getClass().getDeclaredField("value");
  }

  @Override
  public void run() throws Throwable {
    for (int pass = 0; pass < 100; ++pass) {
      field.setInt(this, 1066); // OP
      field.setInt(this, 1066); // OP
      field.setInt(this, 1066); // OP
      field.setInt(this, 1066); // OP
      field.setInt(this, 1066); // OP
      field.setInt(this, 1066); // OP
      field.setInt(this, 1066); // OP
      field.setInt(this, 1066); // OP
      field.setInt(this, 1066); // OP
      field.setInt(this, 1066); // OP
      field.setInt(this, 1066); // OP
      field.setInt(this, 1066); // OP
      field.setInt(this, 1066); // OP
      field.setInt(this, 1066); // OP
      field.setInt(this, 1066); // OP
      field.setInt(this, 1066); // OP
      field.setInt(this, 1066); // OP
      field.setInt(this, 1066); // OP
      field.setInt(this, 1066); // OP
      field.setInt(this, 1066); // OP
      field.setInt(this, 1066); // OP
      field.setInt(this, 1066); // OP
      field.setInt(this, 1066); // OP
      field.setInt(this, 1066); // OP
      field.setInt(this, 1066); // OP
      field.setInt(this, 1066); // OP
      field.setInt(this, 1066); // OP
      field.setInt(this, 1066); // OP
      field.setInt(this, 1066); // OP
      field.setInt(this, 1066); // OP
    }
  }

  @Override
  public int innerIterations() {
      return 3000;
  }
}

class ReflectSetFieldStringBenchmark extends MicroBenchmark {
  Field field;
  String value;

  ReflectSetFieldStringBenchmark() throws Throwable {
    field = this.getClass().getDeclaredField("value");
  }

  @Override
  public void run() throws Throwable {
    for (int pass = 0; pass < 100; ++pass) {
      field.set(this, "Push and pop"); // OP
      field.set(this, "Push and pop"); // OP
      field.set(this, "Push and pop"); // OP
      field.set(this, "Push and pop"); // OP
      field.set(this, "Push and pop"); // OP
      field.set(this, "Push and pop"); // OP
      field.set(this, "Push and pop"); // OP
      field.set(this, "Push and pop"); // OP
      field.set(this, "Push and pop"); // OP
      field.set(this, "Push and pop"); // OP
      field.set(this, "Push and pop"); // OP
      field.set(this, "Push and pop"); // OP
      field.set(this, "Push and pop"); // OP
      field.set(this, "Push and pop"); // OP
      field.set(this, "Push and pop"); // OP
      field.set(this, "Push and pop"); // OP
      field.set(this, "Push and pop"); // OP
      field.set(this, "Push and pop"); // OP
      field.set(this, "Push and pop"); // OP
      field.set(this, "Push and pop"); // OP
      field.set(this, "Push and pop"); // OP
      field.set(this, "Push and pop"); // OP
      field.set(this, "Push and pop"); // OP
      field.set(this, "Push and pop"); // OP
      field.set(this, "Push and pop"); // OP
      field.set(this, "Push and pop"); // OP
      field.set(this, "Push and pop"); // OP
      field.set(this, "Push and pop"); // OP
      field.set(this, "Push and pop"); // OP
      field.set(this, "Push and pop"); // OP
    }
  }

  @Override
  public int innerIterations() {
      return 3000;
  }
}

class UnsafeGetStaticFieldIntBenchmark extends UnsafeMicroBenchmark {
  long offset;
  static int value;

  UnsafeGetStaticFieldIntBenchmark() throws Throwable {
    Field field = this.getClass().getDeclaredField("value");
    offset = getStaticFieldOffset(field);
  }

  @Override
  public void run() throws Throwable {
    int x;
    for (int pass = 0; pass < 100; ++pass) {
      x = theUnsafe.getInt(this.getClass(), offset); // OP
      x = theUnsafe.getInt(this.getClass(), offset); // OP
      x = theUnsafe.getInt(this.getClass(), offset); // OP
      x = theUnsafe.getInt(this.getClass(), offset); // OP
      x = theUnsafe.getInt(this.getClass(), offset); // OP
      x = theUnsafe.getInt(this.getClass(), offset); // OP
      x = theUnsafe.getInt(this.getClass(), offset); // OP
      x = theUnsafe.getInt(this.getClass(), offset); // OP
      x = theUnsafe.getInt(this.getClass(), offset); // OP
      x = theUnsafe.getInt(this.getClass(), offset); // OP
      x = theUnsafe.getInt(this.getClass(), offset); // OP
      x = theUnsafe.getInt(this.getClass(), offset); // OP
      x = theUnsafe.getInt(this.getClass(), offset); // OP
      x = theUnsafe.getInt(this.getClass(), offset); // OP
      x = theUnsafe.getInt(this.getClass(), offset); // OP
      x = theUnsafe.getInt(this.getClass(), offset); // OP
      x = theUnsafe.getInt(this.getClass(), offset); // OP
      x = theUnsafe.getInt(this.getClass(), offset); // OP
      x = theUnsafe.getInt(this.getClass(), offset); // OP
      x = theUnsafe.getInt(this.getClass(), offset); // OP
      x = theUnsafe.getInt(this.getClass(), offset); // OP
      x = theUnsafe.getInt(this.getClass(), offset); // OP
      x = theUnsafe.getInt(this.getClass(), offset); // OP
      x = theUnsafe.getInt(this.getClass(), offset); // OP
      x = theUnsafe.getInt(this.getClass(), offset); // OP
      x = theUnsafe.getInt(this.getClass(), offset); // OP
      x = theUnsafe.getInt(this.getClass(), offset); // OP
      x = theUnsafe.getInt(this.getClass(), offset); // OP
      x = theUnsafe.getInt(this.getClass(), offset); // OP
      x = theUnsafe.getInt(this.getClass(), offset); // OP
    }
  }

  @Override
  public int innerIterations() {
      return 3000;
  }
}

class UnsafeGetStaticFieldStringBenchmark extends UnsafeMicroBenchmark {
  long offset;
  static String value = "hello world!";

  UnsafeGetStaticFieldStringBenchmark() throws Throwable {
    Field field = this.getClass().getDeclaredField("value");
    offset = getStaticFieldOffset(field);
  }

  @Override
  public void run() throws Throwable {
    String s;
    for (int pass = 0; pass < 100; ++pass) {
      s = (String) theUnsafe.getObject(this.getClass(), offset); // OP
      s = (String) theUnsafe.getObject(this.getClass(), offset); // OP
      s = (String) theUnsafe.getObject(this.getClass(), offset); // OP
      s = (String) theUnsafe.getObject(this.getClass(), offset); // OP
      s = (String) theUnsafe.getObject(this.getClass(), offset); // OP
      s = (String) theUnsafe.getObject(this.getClass(), offset); // OP
      s = (String) theUnsafe.getObject(this.getClass(), offset); // OP
      s = (String) theUnsafe.getObject(this.getClass(), offset); // OP
      s = (String) theUnsafe.getObject(this.getClass(), offset); // OP
      s = (String) theUnsafe.getObject(this.getClass(), offset); // OP
      s = (String) theUnsafe.getObject(this.getClass(), offset); // OP
      s = (String) theUnsafe.getObject(this.getClass(), offset); // OP
      s = (String) theUnsafe.getObject(this.getClass(), offset); // OP
      s = (String) theUnsafe.getObject(this.getClass(), offset); // OP
      s = (String) theUnsafe.getObject(this.getClass(), offset); // OP
      s = (String) theUnsafe.getObject(this.getClass(), offset); // OP
      s = (String) theUnsafe.getObject(this.getClass(), offset); // OP
      s = (String) theUnsafe.getObject(this.getClass(), offset); // OP
      s = (String) theUnsafe.getObject(this.getClass(), offset); // OP
      s = (String) theUnsafe.getObject(this.getClass(), offset); // OP
      s = (String) theUnsafe.getObject(this.getClass(), offset); // OP
      s = (String) theUnsafe.getObject(this.getClass(), offset); // OP
      s = (String) theUnsafe.getObject(this.getClass(), offset); // OP
      s = (String) theUnsafe.getObject(this.getClass(), offset); // OP
      s = (String) theUnsafe.getObject(this.getClass(), offset); // OP
      s = (String) theUnsafe.getObject(this.getClass(), offset); // OP
      s = (String) theUnsafe.getObject(this.getClass(), offset); // OP
      s = (String) theUnsafe.getObject(this.getClass(), offset); // OP
      s = (String) theUnsafe.getObject(this.getClass(), offset); // OP
      s = (String) theUnsafe.getObject(this.getClass(), offset); // OP
    }
  }

  @Override
  public int innerIterations() {
      return 3000;
  }
}

class UnsafeGetFieldIntBenchmark extends UnsafeMicroBenchmark {
  long offset;
  int value;

  UnsafeGetFieldIntBenchmark() throws Throwable {
    Field field = this.getClass().getDeclaredField("value");
    offset = getFieldOffset(field);
  }

  @Override
  public void run() throws Throwable {
    int x;
    for (int pass = 0; pass < 100; ++pass) {
      x = theUnsafe.getInt(this, offset); // OP
      x = theUnsafe.getInt(this, offset); // OP
      x = theUnsafe.getInt(this, offset); // OP
      x = theUnsafe.getInt(this, offset); // OP
      x = theUnsafe.getInt(this, offset); // OP
      x = theUnsafe.getInt(this, offset); // OP
      x = theUnsafe.getInt(this, offset); // OP
      x = theUnsafe.getInt(this, offset); // OP
      x = theUnsafe.getInt(this, offset); // OP
      x = theUnsafe.getInt(this, offset); // OP
      x = theUnsafe.getInt(this, offset); // OP
      x = theUnsafe.getInt(this, offset); // OP
      x = theUnsafe.getInt(this, offset); // OP
      x = theUnsafe.getInt(this, offset); // OP
      x = theUnsafe.getInt(this, offset); // OP
      x = theUnsafe.getInt(this, offset); // OP
      x = theUnsafe.getInt(this, offset); // OP
      x = theUnsafe.getInt(this, offset); // OP
      x = theUnsafe.getInt(this, offset); // OP
      x = theUnsafe.getInt(this, offset); // OP
      x = theUnsafe.getInt(this, offset); // OP
      x = theUnsafe.getInt(this, offset); // OP
      x = theUnsafe.getInt(this, offset); // OP
      x = theUnsafe.getInt(this, offset); // OP
      x = theUnsafe.getInt(this, offset); // OP
      x = theUnsafe.getInt(this, offset); // OP
      x = theUnsafe.getInt(this, offset); // OP
      x = theUnsafe.getInt(this, offset); // OP
      x = theUnsafe.getInt(this, offset); // OP
      x = theUnsafe.getInt(this, offset); // OP
    }
  }

  @Override
  public int innerIterations() {
      return 3000;
  }
}

class UnsafeGetFieldStringBenchmark extends UnsafeMicroBenchmark {
  long offset;
  String value = "hello world!";

  UnsafeGetFieldStringBenchmark() throws Throwable {
    Field field = this.getClass().getDeclaredField("value");
    offset = getFieldOffset(field);
  }

  @Override
  public void run() throws Throwable {
    String s;
    for (int pass = 0; pass < 100; ++pass) {
      s = (String) theUnsafe.getObject(this, offset); // OP
      s = (String) theUnsafe.getObject(this, offset); // OP
      s = (String) theUnsafe.getObject(this, offset); // OP
      s = (String) theUnsafe.getObject(this, offset); // OP
      s = (String) theUnsafe.getObject(this, offset); // OP
      s = (String) theUnsafe.getObject(this, offset); // OP
      s = (String) theUnsafe.getObject(this, offset); // OP
      s = (String) theUnsafe.getObject(this, offset); // OP
      s = (String) theUnsafe.getObject(this, offset); // OP
      s = (String) theUnsafe.getObject(this, offset); // OP
      s = (String) theUnsafe.getObject(this, offset); // OP
      s = (String) theUnsafe.getObject(this, offset); // OP
      s = (String) theUnsafe.getObject(this, offset); // OP
      s = (String) theUnsafe.getObject(this, offset); // OP
      s = (String) theUnsafe.getObject(this, offset); // OP
      s = (String) theUnsafe.getObject(this, offset); // OP
      s = (String) theUnsafe.getObject(this, offset); // OP
      s = (String) theUnsafe.getObject(this, offset); // OP
      s = (String) theUnsafe.getObject(this, offset); // OP
      s = (String) theUnsafe.getObject(this, offset); // OP
      s = (String) theUnsafe.getObject(this, offset); // OP
      s = (String) theUnsafe.getObject(this, offset); // OP
      s = (String) theUnsafe.getObject(this, offset); // OP
      s = (String) theUnsafe.getObject(this, offset); // OP
      s = (String) theUnsafe.getObject(this, offset); // OP
      s = (String) theUnsafe.getObject(this, offset); // OP
      s = (String) theUnsafe.getObject(this, offset); // OP
      s = (String) theUnsafe.getObject(this, offset); // OP
      s = (String) theUnsafe.getObject(this, offset); // OP
      s = (String) theUnsafe.getObject(this, offset); // OP
    }
  }

  @Override
  public int innerIterations() {
      return 3000;
  }
}

class UnsafePutStaticFieldIntBenchmark extends UnsafeMicroBenchmark {
  long offset;
  static int value;

  UnsafePutStaticFieldIntBenchmark() throws Throwable {
    Field field = this.getClass().getDeclaredField("value");
    offset = getStaticFieldOffset(field);
  }

  @Override
  public void run() throws Throwable {
    for (int pass = 0; pass < 100; ++pass) {
      theUnsafe.putInt(this.getClass(), offset, 42); // OP
      theUnsafe.putInt(this.getClass(), offset, 42); // OP
      theUnsafe.putInt(this.getClass(), offset, 42); // OP
      theUnsafe.putInt(this.getClass(), offset, 42); // OP
      theUnsafe.putInt(this.getClass(), offset, 42); // OP
      theUnsafe.putInt(this.getClass(), offset, 42); // OP
      theUnsafe.putInt(this.getClass(), offset, 42); // OP
      theUnsafe.putInt(this.getClass(), offset, 42); // OP
      theUnsafe.putInt(this.getClass(), offset, 42); // OP
      theUnsafe.putInt(this.getClass(), offset, 42); // OP
      theUnsafe.putInt(this.getClass(), offset, 42); // OP
      theUnsafe.putInt(this.getClass(), offset, 42); // OP
      theUnsafe.putInt(this.getClass(), offset, 42); // OP
      theUnsafe.putInt(this.getClass(), offset, 42); // OP
      theUnsafe.putInt(this.getClass(), offset, 42); // OP
      theUnsafe.putInt(this.getClass(), offset, 42); // OP
      theUnsafe.putInt(this.getClass(), offset, 42); // OP
      theUnsafe.putInt(this.getClass(), offset, 42); // OP
      theUnsafe.putInt(this.getClass(), offset, 42); // OP
      theUnsafe.putInt(this.getClass(), offset, 42); // OP
      theUnsafe.putInt(this.getClass(), offset, 42); // OP
      theUnsafe.putInt(this.getClass(), offset, 42); // OP
      theUnsafe.putInt(this.getClass(), offset, 42); // OP
      theUnsafe.putInt(this.getClass(), offset, 42); // OP
      theUnsafe.putInt(this.getClass(), offset, 42); // OP
      theUnsafe.putInt(this.getClass(), offset, 42); // OP
      theUnsafe.putInt(this.getClass(), offset, 42); // OP
      theUnsafe.putInt(this.getClass(), offset, 42); // OP
      theUnsafe.putInt(this.getClass(), offset, 42); // OP
      theUnsafe.putInt(this.getClass(), offset, 42); // OP
    }
  }

  @Override
  public int innerIterations() {
      return 3000;
  }
}

class UnsafePutStaticFieldStringBenchmark extends UnsafeMicroBenchmark {
  long offset;
  static String value = "hello world!";

  UnsafePutStaticFieldStringBenchmark() throws Throwable {
    Field field = this.getClass().getDeclaredField("value");
    offset = getStaticFieldOffset(field);
  }

  @Override
  public void run() throws Throwable {
    for (int pass = 0; pass < 100; ++pass) {
      theUnsafe.putObject(this.getClass(), offset, "ciao"); // OP
      theUnsafe.putObject(this.getClass(), offset, "ciao"); // OP
      theUnsafe.putObject(this.getClass(), offset, "ciao"); // OP
      theUnsafe.putObject(this.getClass(), offset, "ciao"); // OP
      theUnsafe.putObject(this.getClass(), offset, "ciao"); // OP
      theUnsafe.putObject(this.getClass(), offset, "ciao"); // OP
      theUnsafe.putObject(this.getClass(), offset, "ciao"); // OP
      theUnsafe.putObject(this.getClass(), offset, "ciao"); // OP
      theUnsafe.putObject(this.getClass(), offset, "ciao"); // OP
      theUnsafe.putObject(this.getClass(), offset, "ciao"); // OP
      theUnsafe.putObject(this.getClass(), offset, "ciao"); // OP
      theUnsafe.putObject(this.getClass(), offset, "ciao"); // OP
      theUnsafe.putObject(this.getClass(), offset, "ciao"); // OP
      theUnsafe.putObject(this.getClass(), offset, "ciao"); // OP
      theUnsafe.putObject(this.getClass(), offset, "ciao"); // OP
      theUnsafe.putObject(this.getClass(), offset, "ciao"); // OP
      theUnsafe.putObject(this.getClass(), offset, "ciao"); // OP
      theUnsafe.putObject(this.getClass(), offset, "ciao"); // OP
      theUnsafe.putObject(this.getClass(), offset, "ciao"); // OP
      theUnsafe.putObject(this.getClass(), offset, "ciao"); // OP
      theUnsafe.putObject(this.getClass(), offset, "ciao"); // OP
      theUnsafe.putObject(this.getClass(), offset, "ciao"); // OP
      theUnsafe.putObject(this.getClass(), offset, "ciao"); // OP
      theUnsafe.putObject(this.getClass(), offset, "ciao"); // OP
      theUnsafe.putObject(this.getClass(), offset, "ciao"); // OP
      theUnsafe.putObject(this.getClass(), offset, "ciao"); // OP
      theUnsafe.putObject(this.getClass(), offset, "ciao"); // OP
      theUnsafe.putObject(this.getClass(), offset, "ciao"); // OP
      theUnsafe.putObject(this.getClass(), offset, "ciao"); // OP
      theUnsafe.putObject(this.getClass(), offset, "ciao"); // OP
    }
  }

  @Override
  public int innerIterations() {
      return 3000;
  }
}

class UnsafePutFieldIntBenchmark extends UnsafeMicroBenchmark {
  long offset;
  int value;

  UnsafePutFieldIntBenchmark() throws Throwable {
    Field field = this.getClass().getDeclaredField("value");
    offset = getFieldOffset(field);
  }

  @Override
  public void run() throws Throwable {
    for (int pass = 0; pass < 100; ++pass) {
      theUnsafe.putInt(this, offset, 1000); // OP
      theUnsafe.putInt(this, offset, 1000); // OP
      theUnsafe.putInt(this, offset, 1000); // OP
      theUnsafe.putInt(this, offset, 1000); // OP
      theUnsafe.putInt(this, offset, 1000); // OP
      theUnsafe.putInt(this, offset, 1000); // OP
      theUnsafe.putInt(this, offset, 1000); // OP
      theUnsafe.putInt(this, offset, 1000); // OP
      theUnsafe.putInt(this, offset, 1000); // OP
      theUnsafe.putInt(this, offset, 1000); // OP
      theUnsafe.putInt(this, offset, 1000); // OP
      theUnsafe.putInt(this, offset, 1000); // OP
      theUnsafe.putInt(this, offset, 1000); // OP
      theUnsafe.putInt(this, offset, 1000); // OP
      theUnsafe.putInt(this, offset, 1000); // OP
      theUnsafe.putInt(this, offset, 1000); // OP
      theUnsafe.putInt(this, offset, 1000); // OP
      theUnsafe.putInt(this, offset, 1000); // OP
      theUnsafe.putInt(this, offset, 1000); // OP
      theUnsafe.putInt(this, offset, 1000); // OP
      theUnsafe.putInt(this, offset, 1000); // OP
      theUnsafe.putInt(this, offset, 1000); // OP
      theUnsafe.putInt(this, offset, 1000); // OP
      theUnsafe.putInt(this, offset, 1000); // OP
      theUnsafe.putInt(this, offset, 1000); // OP
      theUnsafe.putInt(this, offset, 1000); // OP
      theUnsafe.putInt(this, offset, 1000); // OP
      theUnsafe.putInt(this, offset, 1000); // OP
      theUnsafe.putInt(this, offset, 1000); // OP
      theUnsafe.putInt(this, offset, 1000); // OP
    }
  }

  @Override
  public int innerIterations() {
      return 3000;
  }
}

class UnsafePutFieldStringBenchmark extends UnsafeMicroBenchmark {
  long offset;
  String value = "hello world!";

  UnsafePutFieldStringBenchmark() throws Throwable {
    Field field = this.getClass().getDeclaredField("value");
    offset = getFieldOffset(field);
  }

  @Override
  public void run() throws Throwable {
    for (int pass = 0; pass < 100; ++pass) {
      theUnsafe.putObject(this, offset, "goodbye cruel world"); // OP
      theUnsafe.putObject(this, offset, "goodbye cruel world"); // OP
      theUnsafe.putObject(this, offset, "goodbye cruel world"); // OP
      theUnsafe.putObject(this, offset, "goodbye cruel world"); // OP
      theUnsafe.putObject(this, offset, "goodbye cruel world"); // OP
      theUnsafe.putObject(this, offset, "goodbye cruel world"); // OP
      theUnsafe.putObject(this, offset, "goodbye cruel world"); // OP
      theUnsafe.putObject(this, offset, "goodbye cruel world"); // OP
      theUnsafe.putObject(this, offset, "goodbye cruel world"); // OP
      theUnsafe.putObject(this, offset, "goodbye cruel world"); // OP
      theUnsafe.putObject(this, offset, "goodbye cruel world"); // OP
      theUnsafe.putObject(this, offset, "goodbye cruel world"); // OP
      theUnsafe.putObject(this, offset, "goodbye cruel world"); // OP
      theUnsafe.putObject(this, offset, "goodbye cruel world"); // OP
      theUnsafe.putObject(this, offset, "goodbye cruel world"); // OP
      theUnsafe.putObject(this, offset, "goodbye cruel world"); // OP
      theUnsafe.putObject(this, offset, "goodbye cruel world"); // OP
      theUnsafe.putObject(this, offset, "goodbye cruel world"); // OP
      theUnsafe.putObject(this, offset, "goodbye cruel world"); // OP
      theUnsafe.putObject(this, offset, "goodbye cruel world"); // OP
      theUnsafe.putObject(this, offset, "goodbye cruel world"); // OP
      theUnsafe.putObject(this, offset, "goodbye cruel world"); // OP
      theUnsafe.putObject(this, offset, "goodbye cruel world"); // OP
      theUnsafe.putObject(this, offset, "goodbye cruel world"); // OP
      theUnsafe.putObject(this, offset, "goodbye cruel world"); // OP
      theUnsafe.putObject(this, offset, "goodbye cruel world"); // OP
      theUnsafe.putObject(this, offset, "goodbye cruel world"); // OP
      theUnsafe.putObject(this, offset, "goodbye cruel world"); // OP
      theUnsafe.putObject(this, offset, "goodbye cruel world"); // OP
      theUnsafe.putObject(this, offset, "goodbye cruel world"); // OP
    }
  }

  @Override
  public int innerIterations() {
      return 3000;
  }
}

class UnsafeCompareAndSwapStaticFieldIntBenchmark extends UnsafeMicroBenchmark {
  long offset;
  static int value = 42;

  UnsafeCompareAndSwapStaticFieldIntBenchmark() throws Throwable {
    Field field = this.getClass().getDeclaredField("value");
    offset = getStaticFieldOffset(field);
  }

  @Override
  public void run() throws Throwable {
    int newValue = ~value;
    for (int pass = 0; pass < 100; ++pass) {
      theUnsafe.compareAndSwapInt(this.getClass(), offset, value, newValue); // OP
      theUnsafe.compareAndSwapInt(this.getClass(), offset, value, newValue); // OP
      theUnsafe.compareAndSwapInt(this.getClass(), offset, value, newValue); // OP
      theUnsafe.compareAndSwapInt(this.getClass(), offset, value, newValue); // OP
      theUnsafe.compareAndSwapInt(this.getClass(), offset, value, newValue); // OP
      theUnsafe.compareAndSwapInt(this.getClass(), offset, value, newValue); // OP
      theUnsafe.compareAndSwapInt(this.getClass(), offset, value, newValue); // OP
      theUnsafe.compareAndSwapInt(this.getClass(), offset, value, newValue); // OP
      theUnsafe.compareAndSwapInt(this.getClass(), offset, value, newValue); // OP
      theUnsafe.compareAndSwapInt(this.getClass(), offset, value, newValue); // OP
      theUnsafe.compareAndSwapInt(this.getClass(), offset, value, newValue); // OP
      theUnsafe.compareAndSwapInt(this.getClass(), offset, value, newValue); // OP
      theUnsafe.compareAndSwapInt(this.getClass(), offset, value, newValue); // OP
      theUnsafe.compareAndSwapInt(this.getClass(), offset, value, newValue); // OP
      theUnsafe.compareAndSwapInt(this.getClass(), offset, value, newValue); // OP
      theUnsafe.compareAndSwapInt(this.getClass(), offset, value, newValue); // OP
      theUnsafe.compareAndSwapInt(this.getClass(), offset, value, newValue); // OP
      theUnsafe.compareAndSwapInt(this.getClass(), offset, value, newValue); // OP
      theUnsafe.compareAndSwapInt(this.getClass(), offset, value, newValue); // OP
      theUnsafe.compareAndSwapInt(this.getClass(), offset, value, newValue); // OP
      theUnsafe.compareAndSwapInt(this.getClass(), offset, value, newValue); // OP
      theUnsafe.compareAndSwapInt(this.getClass(), offset, value, newValue); // OP
      theUnsafe.compareAndSwapInt(this.getClass(), offset, value, newValue); // OP
      theUnsafe.compareAndSwapInt(this.getClass(), offset, value, newValue); // OP
      theUnsafe.compareAndSwapInt(this.getClass(), offset, value, newValue); // OP
      theUnsafe.compareAndSwapInt(this.getClass(), offset, value, newValue); // OP
      theUnsafe.compareAndSwapInt(this.getClass(), offset, value, newValue); // OP
      theUnsafe.compareAndSwapInt(this.getClass(), offset, value, newValue); // OP
      theUnsafe.compareAndSwapInt(this.getClass(), offset, value, newValue); // OP
      theUnsafe.compareAndSwapInt(this.getClass(), offset, value, newValue); // OP
    }
  }

  @Override
  public int innerIterations() {
      return 3000;
  }
}

class UnsafeCompareAndSwapStaticFieldStringBenchmark extends UnsafeMicroBenchmark {
  static final String FIELD_VALUE = "field value";

  long offset;
  static String value = null;

  UnsafeCompareAndSwapStaticFieldStringBenchmark() throws Throwable {
    Field field = this.getClass().getDeclaredField("value");
    offset = getStaticFieldOffset(field);
  }

  @Override
  public void run() throws Throwable {
    String newValue = (value != null) ? null : FIELD_VALUE;
    for (int pass = 0; pass < 100; ++pass) {
      theUnsafe.compareAndSwapObject(this.getClass(), offset, value, newValue); // OP
      theUnsafe.compareAndSwapObject(this.getClass(), offset, value, newValue); // OP
      theUnsafe.compareAndSwapObject(this.getClass(), offset, value, newValue); // OP
      theUnsafe.compareAndSwapObject(this.getClass(), offset, value, newValue); // OP
      theUnsafe.compareAndSwapObject(this.getClass(), offset, value, newValue); // OP
      theUnsafe.compareAndSwapObject(this.getClass(), offset, value, newValue); // OP
      theUnsafe.compareAndSwapObject(this.getClass(), offset, value, newValue); // OP
      theUnsafe.compareAndSwapObject(this.getClass(), offset, value, newValue); // OP
      theUnsafe.compareAndSwapObject(this.getClass(), offset, value, newValue); // OP
      theUnsafe.compareAndSwapObject(this.getClass(), offset, value, newValue); // OP
      theUnsafe.compareAndSwapObject(this.getClass(), offset, value, newValue); // OP
      theUnsafe.compareAndSwapObject(this.getClass(), offset, value, newValue); // OP
      theUnsafe.compareAndSwapObject(this.getClass(), offset, value, newValue); // OP
      theUnsafe.compareAndSwapObject(this.getClass(), offset, value, newValue); // OP
      theUnsafe.compareAndSwapObject(this.getClass(), offset, value, newValue); // OP
      theUnsafe.compareAndSwapObject(this.getClass(), offset, value, newValue); // OP
      theUnsafe.compareAndSwapObject(this.getClass(), offset, value, newValue); // OP
      theUnsafe.compareAndSwapObject(this.getClass(), offset, value, newValue); // OP
      theUnsafe.compareAndSwapObject(this.getClass(), offset, value, newValue); // OP
      theUnsafe.compareAndSwapObject(this.getClass(), offset, value, newValue); // OP
      theUnsafe.compareAndSwapObject(this.getClass(), offset, value, newValue); // OP
      theUnsafe.compareAndSwapObject(this.getClass(), offset, value, newValue); // OP
      theUnsafe.compareAndSwapObject(this.getClass(), offset, value, newValue); // OP
      theUnsafe.compareAndSwapObject(this.getClass(), offset, value, newValue); // OP
      theUnsafe.compareAndSwapObject(this.getClass(), offset, value, newValue); // OP
      theUnsafe.compareAndSwapObject(this.getClass(), offset, value, newValue); // OP
      theUnsafe.compareAndSwapObject(this.getClass(), offset, value, newValue); // OP
      theUnsafe.compareAndSwapObject(this.getClass(), offset, value, newValue); // OP
      theUnsafe.compareAndSwapObject(this.getClass(), offset, value, newValue); // OP
      theUnsafe.compareAndSwapObject(this.getClass(), offset, value, newValue); // OP
    }
  }

  @Override
  public int innerIterations() {
      return 3000;
  }
}

class UnsafeCompareAndSwapFieldIntBenchmark extends UnsafeMicroBenchmark {
  static final int FIELD_VALUE = 42;

  long offset;
  int value = FIELD_VALUE;

  UnsafeCompareAndSwapFieldIntBenchmark() throws Throwable {
    Field field = this.getClass().getDeclaredField("value");
    offset = getFieldOffset(field);
  }

  @Override
  public void run() throws Throwable {
    for (int pass = 0; pass < 100; ++pass) {
      theUnsafe.compareAndSwapInt(this, offset, value, ~value); // OP
      theUnsafe.compareAndSwapInt(this, offset, value, ~value); // OP
      theUnsafe.compareAndSwapInt(this, offset, value, ~value); // OP
      theUnsafe.compareAndSwapInt(this, offset, value, ~value); // OP
      theUnsafe.compareAndSwapInt(this, offset, value, ~value); // OP
      theUnsafe.compareAndSwapInt(this, offset, value, ~value); // OP
      theUnsafe.compareAndSwapInt(this, offset, value, ~value); // OP
      theUnsafe.compareAndSwapInt(this, offset, value, ~value); // OP
      theUnsafe.compareAndSwapInt(this, offset, value, ~value); // OP
      theUnsafe.compareAndSwapInt(this, offset, value, ~value); // OP
      theUnsafe.compareAndSwapInt(this, offset, value, ~value); // OP
      theUnsafe.compareAndSwapInt(this, offset, value, ~value); // OP
      theUnsafe.compareAndSwapInt(this, offset, value, ~value); // OP
      theUnsafe.compareAndSwapInt(this, offset, value, ~value); // OP
      theUnsafe.compareAndSwapInt(this, offset, value, ~value); // OP
      theUnsafe.compareAndSwapInt(this, offset, value, ~value); // OP
      theUnsafe.compareAndSwapInt(this, offset, value, ~value); // OP
      theUnsafe.compareAndSwapInt(this, offset, value, ~value); // OP
      theUnsafe.compareAndSwapInt(this, offset, value, ~value); // OP
      theUnsafe.compareAndSwapInt(this, offset, value, ~value); // OP
      theUnsafe.compareAndSwapInt(this, offset, value, ~value); // OP
      theUnsafe.compareAndSwapInt(this, offset, value, ~value); // OP
      theUnsafe.compareAndSwapInt(this, offset, value, ~value); // OP
      theUnsafe.compareAndSwapInt(this, offset, value, ~value); // OP
      theUnsafe.compareAndSwapInt(this, offset, value, ~value); // OP
      theUnsafe.compareAndSwapInt(this, offset, value, ~value); // OP
      theUnsafe.compareAndSwapInt(this, offset, value, ~value); // OP
      theUnsafe.compareAndSwapInt(this, offset, value, ~value); // OP
      theUnsafe.compareAndSwapInt(this, offset, value, ~value); // OP
      theUnsafe.compareAndSwapInt(this, offset, value, ~value); // OP
    }
  }

  @Override
  public int innerIterations() {
      return 3000;
  }
}

class UnsafeCompareAndSwapFieldStringBenchmark extends UnsafeMicroBenchmark {
  static final String FIELD_VALUE = "field value";

  long offset;
  String value = null;

  UnsafeCompareAndSwapFieldStringBenchmark() throws Throwable {
    Field field = this.getClass().getDeclaredField("value");
    offset = getFieldOffset(field);
  }

  @Override
  public void run() throws Throwable {
    String newValue = (value != null) ? null : FIELD_VALUE;
    for (int pass = 0; pass < 100; ++pass) {
      theUnsafe.compareAndSwapObject(this, offset, value, newValue); // OP
      theUnsafe.compareAndSwapObject(this, offset, value, newValue); // OP
      theUnsafe.compareAndSwapObject(this, offset, value, newValue); // OP
      theUnsafe.compareAndSwapObject(this, offset, value, newValue); // OP
      theUnsafe.compareAndSwapObject(this, offset, value, newValue); // OP
      theUnsafe.compareAndSwapObject(this, offset, value, newValue); // OP
      theUnsafe.compareAndSwapObject(this, offset, value, newValue); // OP
      theUnsafe.compareAndSwapObject(this, offset, value, newValue); // OP
      theUnsafe.compareAndSwapObject(this, offset, value, newValue); // OP
      theUnsafe.compareAndSwapObject(this, offset, value, newValue); // OP
      theUnsafe.compareAndSwapObject(this, offset, value, newValue); // OP
      theUnsafe.compareAndSwapObject(this, offset, value, newValue); // OP
      theUnsafe.compareAndSwapObject(this, offset, value, newValue); // OP
      theUnsafe.compareAndSwapObject(this, offset, value, newValue); // OP
      theUnsafe.compareAndSwapObject(this, offset, value, newValue); // OP
      theUnsafe.compareAndSwapObject(this, offset, value, newValue); // OP
      theUnsafe.compareAndSwapObject(this, offset, value, newValue); // OP
      theUnsafe.compareAndSwapObject(this, offset, value, newValue); // OP
      theUnsafe.compareAndSwapObject(this, offset, value, newValue); // OP
      theUnsafe.compareAndSwapObject(this, offset, value, newValue); // OP
      theUnsafe.compareAndSwapObject(this, offset, value, newValue); // OP
      theUnsafe.compareAndSwapObject(this, offset, value, newValue); // OP
      theUnsafe.compareAndSwapObject(this, offset, value, newValue); // OP
      theUnsafe.compareAndSwapObject(this, offset, value, newValue); // OP
      theUnsafe.compareAndSwapObject(this, offset, value, newValue); // OP
      theUnsafe.compareAndSwapObject(this, offset, value, newValue); // OP
      theUnsafe.compareAndSwapObject(this, offset, value, newValue); // OP
      theUnsafe.compareAndSwapObject(this, offset, value, newValue); // OP
      theUnsafe.compareAndSwapObject(this, offset, value, newValue); // OP
      theUnsafe.compareAndSwapObject(this, offset, value, newValue); // OP
    }
  }

  @Override
  public int innerIterations() {
      return 3000;
  }
}

class MicroVarHandleBenchmarks {
  static MicroBenchmark[] benchmarks;

  private static void initialize() throws Throwable {
    benchmarks = new MicroBenchmark[] {
      new VarHandleGetStaticFieldIntBenchmark(),
      new VarHandleGetStaticFieldStringBenchmark(),
      new VarHandleGetFieldIntBenchmark(),
      new VarHandleGetFieldStringBenchmark(),
      new VarHandleGetAcquireStaticFieldIntBenchmark(),
      new VarHandleGetAcquireStaticFieldStringBenchmark(),
      new VarHandleGetAcquireFieldIntBenchmark(),
      new VarHandleGetAcquireFieldStringBenchmark(),
      new VarHandleGetOpaqueStaticFieldIntBenchmark(),
      new VarHandleGetOpaqueStaticFieldStringBenchmark(),
      new VarHandleGetOpaqueFieldIntBenchmark(),
      new VarHandleGetOpaqueFieldStringBenchmark(),
      new VarHandleGetVolatileStaticFieldIntBenchmark(),
      new VarHandleGetVolatileStaticFieldStringBenchmark(),
      new VarHandleGetVolatileFieldIntBenchmark(),
      new VarHandleGetVolatileFieldStringBenchmark(),

      new VarHandleSetStaticFieldIntBenchmark(),
      new VarHandleSetStaticFieldStringBenchmark(),
      new VarHandleSetFieldIntBenchmark(),
      new VarHandleSetFieldStringBenchmark(),
      new VarHandleSetVolatileStaticFieldIntBenchmark(),
      new VarHandleSetVolatileStaticFieldStringBenchmark(),
      new VarHandleSetVolatileFieldIntBenchmark(),
      new VarHandleSetVolatileFieldStringBenchmark(),
      new VarHandleSetOpaqueStaticFieldIntBenchmark(),
      new VarHandleSetOpaqueStaticFieldStringBenchmark(),
      new VarHandleSetOpaqueFieldIntBenchmark(),
      new VarHandleSetOpaqueFieldStringBenchmark(),
      new VarHandleSetReleaseStaticFieldIntBenchmark(),
      new VarHandleSetReleaseStaticFieldStringBenchmark(),
      new VarHandleSetReleaseFieldIntBenchmark(),
      new VarHandleSetReleaseFieldStringBenchmark(),

      new VarHandleCompareAndSetStaticFieldIntBenchmark(),
      new VarHandleCompareAndSetStaticFieldStringBenchmark(),
      new VarHandleCompareAndSetFieldIntBenchmark(),
      new VarHandleCompareAndSetFieldStringBenchmark(),
      new VarHandleWeakCompareAndSetStaticFieldIntBenchmark(),
      new VarHandleWeakCompareAndSetStaticFieldStringBenchmark(),
      new VarHandleWeakCompareAndSetFieldIntBenchmark(),
      new VarHandleWeakCompareAndSetFieldStringBenchmark(),
      new VarHandleWeakCompareAndSetPlainStaticFieldIntBenchmark(),
      new VarHandleWeakCompareAndSetPlainStaticFieldStringBenchmark(),
      new VarHandleWeakCompareAndSetPlainFieldIntBenchmark(),
      new VarHandleWeakCompareAndSetPlainFieldStringBenchmark(),
      new VarHandleWeakCompareAndSetAcquireStaticFieldIntBenchmark(),
      new VarHandleWeakCompareAndSetAcquireStaticFieldStringBenchmark(),
      new VarHandleWeakCompareAndSetAcquireFieldIntBenchmark(),
      new VarHandleWeakCompareAndSetAcquireFieldStringBenchmark(),
      new VarHandleWeakCompareAndSetReleaseStaticFieldIntBenchmark(),
      new VarHandleWeakCompareAndSetReleaseStaticFieldStringBenchmark(),
      new VarHandleWeakCompareAndSetReleaseFieldIntBenchmark(),
      new VarHandleWeakCompareAndSetReleaseFieldStringBenchmark(),

      new VarHandleGetAndAddStaticFieldIntBenchmark(),
      new VarHandleGetAndAddFieldIntBenchmark(),
      new VarHandleGetAndAddStaticFieldFloatBenchmark(),
      new VarHandleGetAndAddFieldFloatBenchmark(),
      new VarHandleGetAndAddAcquireStaticFieldIntBenchmark(),
      new VarHandleGetAndAddAcquireFieldIntBenchmark(),
      new VarHandleGetAndAddAcquireStaticFieldFloatBenchmark(),
      new VarHandleGetAndAddAcquireFieldFloatBenchmark(),
      new VarHandleGetAndAddReleaseStaticFieldIntBenchmark(),
      new VarHandleGetAndAddReleaseFieldIntBenchmark(),
      new VarHandleGetAndAddReleaseStaticFieldFloatBenchmark(),
      new VarHandleGetAndAddReleaseFieldFloatBenchmark(),

      new VarHandleGetAndBitwiseOrStaticFieldIntBenchmark(),
      new VarHandleGetAndBitwiseOrFieldIntBenchmark(),
      new VarHandleGetAndBitwiseOrAcquireStaticFieldIntBenchmark(),
      new VarHandleGetAndBitwiseOrAcquireFieldIntBenchmark(),
      new VarHandleGetAndBitwiseOrReleaseStaticFieldIntBenchmark(),
      new VarHandleGetAndBitwiseOrReleaseFieldIntBenchmark(),
      new VarHandleGetAndBitwiseXorStaticFieldIntBenchmark(),
      new VarHandleGetAndBitwiseXorFieldIntBenchmark(),
      new VarHandleGetAndBitwiseXorAcquireStaticFieldIntBenchmark(),
      new VarHandleGetAndBitwiseXorAcquireFieldIntBenchmark(),
      new VarHandleGetAndBitwiseXorReleaseStaticFieldIntBenchmark(),
      new VarHandleGetAndBitwiseXorReleaseFieldIntBenchmark(),
      new VarHandleGetAndBitwiseAndStaticFieldIntBenchmark(),
      new VarHandleGetAndBitwiseAndFieldIntBenchmark(),
      new VarHandleGetAndBitwiseAndAcquireStaticFieldIntBenchmark(),
      new VarHandleGetAndBitwiseAndAcquireFieldIntBenchmark(),
      new VarHandleGetAndBitwiseAndReleaseStaticFieldIntBenchmark(),
      new VarHandleGetAndBitwiseAndReleaseFieldIntBenchmark(),

      new VarHandleGetAndSetStaticFieldIntBenchmark(),
      new VarHandleGetAndSetStaticFieldStringBenchmark(),
      new VarHandleGetAndSetFieldIntBenchmark(),
      new VarHandleGetAndSetFieldStringBenchmark(),
      new VarHandleGetAndSetAcquireStaticFieldIntBenchmark(),
      new VarHandleGetAndSetAcquireStaticFieldStringBenchmark(),
      new VarHandleGetAndSetAcquireFieldIntBenchmark(),
      new VarHandleGetAndSetAcquireFieldStringBenchmark(),
      new VarHandleGetAndSetReleaseStaticFieldIntBenchmark(),
      new VarHandleGetAndSetReleaseStaticFieldStringBenchmark(),
      new VarHandleGetAndSetReleaseFieldIntBenchmark(),
      new VarHandleGetAndSetReleaseFieldStringBenchmark(),

      new VarHandleCompareAndExchangeStaticFieldIntBenchmark(),
      new VarHandleCompareAndExchangeStaticFieldStringBenchmark(),
      new VarHandleCompareAndExchangeFieldIntBenchmark(),
      new VarHandleCompareAndExchangeFieldStringBenchmark(),
      new VarHandleCompareAndExchangeAcquireStaticFieldIntBenchmark(),
      new VarHandleCompareAndExchangeAcquireStaticFieldStringBenchmark(),
      new VarHandleCompareAndExchangeAcquireFieldIntBenchmark(),
      new VarHandleCompareAndExchangeAcquireFieldStringBenchmark(),
      new VarHandleCompareAndExchangeReleaseStaticFieldIntBenchmark(),
      new VarHandleCompareAndExchangeReleaseStaticFieldStringBenchmark(),
      new VarHandleCompareAndExchangeReleaseFieldIntBenchmark(),
      new VarHandleCompareAndExchangeReleaseFieldStringBenchmark(),

      new VarHandleGetArrayElementIntBenchmark(),
      new VarHandleGetArrayElementStringBenchmark(),
      new VarHandleSetArrayElementIntBenchmark(),
      new VarHandleSetArrayElementStringBenchmark(),

      new VarHandleGetByteArrayViewIntBenchmark(),
      new VarHandleSetByteArrayViewIntBenchmark(),
      new VarHandleGetByteArrayViewBigEndianIntBenchmark(),
      new VarHandleSetByteArrayViewBigEndianIntBenchmark(),

      new ReflectGetStaticFieldIntBenchmark(),
      new ReflectGetStaticFieldStringBenchmark(),
      new ReflectGetFieldIntBenchmark(),
      new ReflectGetFieldStringBenchmark(),
      new ReflectSetStaticFieldIntBenchmark(),
      new ReflectSetStaticFieldStringBenchmark(),
      new ReflectSetFieldIntBenchmark(),
      new ReflectSetFieldStringBenchmark(),
      new UnsafeGetStaticFieldIntBenchmark(),
      new UnsafeGetStaticFieldStringBenchmark(),
      new UnsafeGetFieldIntBenchmark(),
      new UnsafeGetFieldStringBenchmark(),
      new UnsafePutStaticFieldIntBenchmark(),
      new UnsafePutStaticFieldStringBenchmark(),
      new UnsafePutFieldIntBenchmark(),
      new UnsafePutFieldStringBenchmark(),
      new UnsafeCompareAndSwapStaticFieldIntBenchmark(),
      new UnsafeCompareAndSwapStaticFieldStringBenchmark(),
      new UnsafeCompareAndSwapFieldIntBenchmark(),
      new UnsafeCompareAndSwapFieldStringBenchmark(),
    };
  }

  private static void runAllBenchmarks() throws Throwable {
    for (MicroBenchmark benchmark : benchmarks) {
      benchmark.report();
    }
  }

  static void main(String[] args) throws Throwable {
    initialize();
    runAllBenchmarks();
  }
}
