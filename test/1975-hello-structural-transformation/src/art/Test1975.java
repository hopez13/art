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

package art;

import java.lang.ref.*;
import java.lang.reflect.*;
import java.util.*;

public class Test1975 {
  public static void run() throws Exception {
    Redefinition.setTestConfiguration(Redefinition.Config.COMMON_REDEFINE);
    doTest();
  }

  private static final boolean PRINT_NONDETERMINISTIC = true;

  public static WeakHashMap<Object, Long> id_nums = new WeakHashMap<>();
  public static long next_id = 0;

  public static String printGeneric(Object o) {
    Long id = id_nums.get(o);
    if (id == null) {
      id = Long.valueOf(next_id++);
      id_nums.put(o, id);
    }
    if (o == null) {
      return "(ID: " + id + ") <NULL>";
    }
    Class oc = o.getClass();
    if (oc.isArray() && oc.getComponentType() == Byte.TYPE) {
      return "(ID: " + id + ") " + Arrays.toString(Arrays.copyOf((byte[]) o, 10)).replace(']', ',') + " ...]";
    } else {
      return "(ID: " + id + ") " + o.toString();
    }
  }

  static void ReadFields() throws Exception {
    for (Field f : Transform1975.class.getFields()) {
      System.out.println(f.toString() + " = " + printGeneric(f.get(null)));
    }
    Transform1975.readFields();
    readNativeFields(Transform1975.class, getNativeFields(Transform1975.class.getFields()));
    Transform1975.doSomething();
    for (Field f : Transform1975.class.getFields()) {
      System.out.println(f.toString() + " = " + printGeneric(f.get(null)));
    }
    Transform1975.readFields();
    readNativeFields(Transform1975.class, getNativeFields(Transform1975.class.getFields()));
  }

  public static void doTest() throws Exception {
    Field[] old_fields = Transform1975.class.getFields();
    long[] old_native_fields = getNativeFields(Transform1975.class.getFields());
    ReadFields();
    Redefinition.doCommonStructuralClassRedefinition(
        Transform1975.class, Transform1975.REDEFINED_DEX_BYTES);
    ReadFields();
    // Now try old-fields
    for (Field f : old_fields) {
      System.out.println("OLD FIELD OBJECT: " + f.toString() + " = " + printGeneric(f.get(null)));
    }
    readNativeFields(Transform1975.class, old_native_fields);
    long[] new_fields = getNativeFields(Transform1975.class.getFields());
    Arrays.sort(old_native_fields);
    Arrays.sort(new_fields);
    boolean different = new_fields.length == old_native_fields.length;
    for (int i = 0; i < old_native_fields.length && !different; i++) {
      different = different || new_fields[i] != old_native_fields[i];
    }
    if (different) {
      System.out.println(
          "Missing expected fields! "
              + Arrays.toString(new_fields)
              + " vs "
              + Arrays.toString(old_native_fields));
    }
  }

  private static void printNativeField(long id, Field f, Object value) {
    System.out.println(
        (PRINT_NONDETERMINISTIC ? "Field id " + id : "Field " + f) + " = " + printGeneric(value));
  }

  public static native long[] getNativeFields(Field[] fields);

  public static native void readNativeFields(Class<?> field_class, long[] sfields);
}
