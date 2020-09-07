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
    int x = (int) vh.get();
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
    String x = (String) vh.get();
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
    int x = (int) vh.get(this);
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
    String x = (String) vh.get(this);
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
    vh.set(FIELD_VALUE);
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
    vh.set(FIELD_VALUE);
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
    vh.set(this, FIELD_VALUE);
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
    vh.set(this, FIELD_VALUE);
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
    boolean success = vh.compareAndSet(field, ~field);
    if (!success) {
      throw new RuntimeException("compareAndSet failed");
    }
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
    boolean success = vh.compareAndSet(field, newValue);
    if (!success) {
      throw new RuntimeException("compareAndSet failed");
    }
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
    boolean success = vh.compareAndSet(this, field, ~field);
    if (!success) {
      throw new RuntimeException("compareAndSet failed");
    }
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
    boolean success = vh.compareAndSet(this, field, newValue);
    if (!success) {
      throw new RuntimeException("compareAndSet failed");
    }
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
    int x = field.getInt(null);
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
    String x = (String) field.get(null);
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
    int x = field.getInt(this);
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
    String x = (String) field.get(this);
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
    field.setInt(null, 42);
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
    field.set(null, "Peace and beans");
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
    field.setInt(this, 1066);
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
    field.set(this, "Push and pop");
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
    int x = theUnsafe.getInt(this.getClass(), offset);
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
    String s = (String) theUnsafe.getObject(this.getClass(), offset);
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
    int x = theUnsafe.getInt(this, offset);
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
    String s = (String) theUnsafe.getObject(this, offset);
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
    theUnsafe.putInt(this.getClass(), offset, 42);
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
    theUnsafe.putObject(this.getClass(), offset, "ciao");
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
    theUnsafe.putInt(this, offset, 1000);
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
    theUnsafe.putObject(this, offset, "goodbye cruel world");
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
    theUnsafe.compareAndSwapInt(this.getClass(), offset, value, newValue);
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
    theUnsafe.compareAndSwapObject(this.getClass(), offset, value, newValue);
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
    theUnsafe.compareAndSwapInt(this, offset, value, ~value);
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
    theUnsafe.compareAndSwapObject(this, offset, value, newValue);
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
      new VarHandleSetStaticFieldIntBenchmark(),
      new VarHandleSetStaticFieldStringBenchmark(),
      new VarHandleSetFieldIntBenchmark(),
      new VarHandleSetFieldStringBenchmark(),
      new VarHandleCompareAndSetStaticFieldIntBenchmark(),
      new VarHandleCompareAndSetStaticFieldStringBenchmark(),
      new VarHandleCompareAndSetFieldIntBenchmark(),
      new VarHandleCompareAndSetFieldStringBenchmark(),
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