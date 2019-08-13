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

import java.lang.reflect.*;
import java.util.*;
import java.lang.ref.*;

public class Test1975 {
  public static void run() throws Exception {
    Redefinition.setTestConfiguration(Redefinition.Config.COMMON_REDEFINE);
    doTest();
  }

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
      return "(ID: " + id + ") " + Arrays.toString((byte[])o);
    } else {
      return "(ID: " + id + ") " + o.toString();
    }
  }

  static void ReadFields() throws Exception {
    for (Field f : Transform1975.class.getFields()) {
      System.out.println(f.toString()  + " = " + printGeneric(f.get(null)));
    }
    Transform1975.readFields();
    Transform1975.doSomething();
    for (Field f : Transform1975.class.getFields()) {
      System.out.println(f.toString()  + " = " + printGeneric(f.get(null)));
    }
    Transform1975.readFields();
  }

  public static void doTest() throws Exception {
    Field[] old_fields = Transform1975.class.getFields();
    ReadFields();
    Redefinition.doCommonStructuralClassRedefinition(Transform1975.class, Transform1975.REDEFINED_DEX_BYTES);
    for (Field f : old_fields) {
      System.out.println("OLD FIELD OBJECT: " + f.toString()  + " = " + printGeneric(f.get(null)));
    }
    ReadFields();
  }
}
