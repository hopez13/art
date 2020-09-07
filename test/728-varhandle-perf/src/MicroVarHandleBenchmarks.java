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
    int x = (int) vh.getAcquire();
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
    String x = (String) vh.getAcquire();
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
    int x = (int) vh.getAcquire(this);
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
    String x = (String) vh.getAcquire(this);
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
    int x = (int) vh.getOpaque();
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
    String x = (String) vh.getOpaque();
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
    int x = (int) vh.getOpaque(this);
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
    String x = (String) vh.getOpaque(this);
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
    int x = (int) vh.getVolatile();
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
    String x = (String) vh.getVolatile();
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
    int x = (int) vh.getVolatile(this);
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
    String x = (String) vh.getVolatile(this);
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
    vh.setVolatile(FIELD_VALUE);
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
    vh.setVolatile(FIELD_VALUE);
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
    vh.setVolatile(this, FIELD_VALUE);
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
    vh.setVolatile(this, FIELD_VALUE);
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
    vh.setOpaque(FIELD_VALUE);
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
    vh.setOpaque(FIELD_VALUE);
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
    vh.setOpaque(this, FIELD_VALUE);
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
    vh.setOpaque(this, FIELD_VALUE);
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
    vh.setRelease(FIELD_VALUE);
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
    vh.setRelease(FIELD_VALUE);
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
    vh.setRelease(this, FIELD_VALUE);
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
    vh.setRelease(this, FIELD_VALUE);
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

class VarHandleWeakCompareAndSetStaticFieldIntBenchmark extends MicroBenchmark {
  static final int FIELD_VALUE = 42;
  static int field = ~FIELD_VALUE;

  VarHandle vh;

  VarHandleWeakCompareAndSetStaticFieldIntBenchmark() throws Throwable {
    vh = MethodHandles.lookup().findStaticVarHandle(this.getClass(), "field", int.class);
  }

  @Override
  public void run() {
    boolean success = vh.weakCompareAndSet(field, ~field);
    if (!success) {
      throw new RuntimeException("compareAndSet failed");
    }
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
    boolean success = vh.weakCompareAndSet(field, newValue);
    if (!success) {
      throw new RuntimeException("compareAndSet failed");
    }
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
    boolean success = vh.weakCompareAndSet(this, field, ~field);
    if (!success) {
      throw new RuntimeException("compareAndSet failed");
    }
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
    boolean success = vh.weakCompareAndSet(this, field, newValue);
    if (!success) {
      throw new RuntimeException("compareAndSet failed");
    }
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
    boolean success = vh.weakCompareAndSetPlain(field, ~field);
    if (!success) {
      throw new RuntimeException("compareAndSet failed");
    }
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
    boolean success = vh.weakCompareAndSetPlain(field, newValue);
    if (!success) {
      throw new RuntimeException("compareAndSet failed");
    }
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
    boolean success = vh.weakCompareAndSetPlain(this, field, ~field);
    if (!success) {
      throw new RuntimeException("compareAndSet failed");
    }
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
    boolean success = vh.weakCompareAndSetPlain(this, field, newValue);
    if (!success) {
      throw new RuntimeException("compareAndSet failed");
    }
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
    boolean success = vh.weakCompareAndSetAcquire(field, ~field);
    if (!success) {
      throw new RuntimeException("compareAndSet failed");
    }
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
    boolean success = vh.weakCompareAndSetAcquire(field, newValue);
    if (!success) {
      throw new RuntimeException("compareAndSet failed");
    }
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
    boolean success = vh.weakCompareAndSetAcquire(this, field, ~field);
    if (!success) {
      throw new RuntimeException("compareAndSet failed");
    }
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
    boolean success = vh.weakCompareAndSetAcquire(this, field, newValue);
    if (!success) {
      throw new RuntimeException("compareAndSet failed");
    }
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
    boolean success = vh.weakCompareAndSetRelease(field, ~field);
    if (!success) {
      throw new RuntimeException("compareAndSet failed");
    }
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
    boolean success = vh.weakCompareAndSetRelease(field, newValue);
    if (!success) {
      throw new RuntimeException("compareAndSet failed");
    }
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
    boolean success = vh.weakCompareAndSetRelease(this, field, ~field);
    if (!success) {
      throw new RuntimeException("compareAndSet failed");
    }
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
    boolean success = vh.weakCompareAndSetRelease(this, field, newValue);
    if (!success) {
      throw new RuntimeException("compareAndSet failed");
    }
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
    int x = (int) vh.getAndAdd(ADD_VALUE);
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
    int x = (int) vh.getAndAdd(this, ADD_VALUE);
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
    float x = (float) vh.getAndAdd(ADD_VALUE);
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
    float x = (float) vh.getAndAdd(this, ADD_VALUE);
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
    int x = (int) vh.getAndBitwiseOr(ADD_VALUE);
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
    int x = (int) vh.getAndBitwiseOr(this, ADD_VALUE);
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
    int x = (int) vh.getAndBitwiseXor(ADD_VALUE);
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
    int x = (int) vh.getAndBitwiseXor(this, ADD_VALUE);
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
    int x = (int) vh.getAndBitwiseAnd(ADD_VALUE);
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
    int x = (int) vh.getAndBitwiseAnd(this, ADD_VALUE);
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
    int x = (int) vh.getAndSet(FIELD_VALUE);
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
    String x = (String) vh.getAndSet(FIELD_VALUE);
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
    int x = (int) vh.getAndSet(this, FIELD_VALUE);
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
    String x = (String) vh.getAndSet(this, FIELD_VALUE);
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
    int result = (int) vh.compareAndExchange(field, newValue);
    if (field != newValue) {
      throw new RuntimeException("compareAndExchange failed");
    }
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
    String result = (String) vh.compareAndExchange(field, newValue);
    if (field != newValue) {
      throw new RuntimeException("compareAndExchange failed");
    }
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
    int result = (int) vh.compareAndExchange(this, field, newValue);
    if (field != newValue) {
      throw new RuntimeException("compareAndExchange failed");
    }
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
    String result = (String) vh.compareAndExchange(this, field, newValue);
    if (field != newValue) {
      throw new RuntimeException("compareAndExchange failed");
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
      new VarHandleGetAndBitwiseOrStaticFieldIntBenchmark(),
      new VarHandleGetAndBitwiseOrFieldIntBenchmark(),
      new VarHandleGetAndBitwiseXorStaticFieldIntBenchmark(),
      new VarHandleGetAndBitwiseXorFieldIntBenchmark(),
      new VarHandleGetAndBitwiseAndStaticFieldIntBenchmark(),
      new VarHandleGetAndBitwiseAndFieldIntBenchmark(),
      new VarHandleGetAndSetStaticFieldIntBenchmark(),
      new VarHandleGetAndSetStaticFieldStringBenchmark(),
      new VarHandleGetAndSetFieldIntBenchmark(),
      new VarHandleGetAndSetFieldStringBenchmark(),
      new VarHandleCompareAndExchangeStaticFieldIntBenchmark(),
      new VarHandleCompareAndExchangeStaticFieldStringBenchmark(),
      new VarHandleCompareAndExchangeFieldIntBenchmark(),
      new VarHandleCompareAndExchangeFieldStringBenchmark(),
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
