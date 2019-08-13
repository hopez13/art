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
      return "(ID: "
          + id
          + ") "
          + Arrays.toString(Arrays.copyOf((byte[]) o, 10)).replace(']', ',')
          + " ...]";
    } else {
      return "(ID: " + id + ") " + o.toString();
    }
  }

  // Since we are adding fields we redefine this class with the Transform1975 class to add new
  // field-reads.
  public static final class ReadTransformFields implements Runnable {
    public void run() {
      System.out.println("Read CUR_CLASS field: " + printGeneric(Transform1975.CUR_CLASS));
      System.out.println(
          "Read REDEFINED_DEX_BYTES field: " + printGeneric(Transform1975.REDEFINED_DEX_BYTES));
    }
  }

  /* Base64 encoded dex file for:
   * public static final class ReadTransformFields implements Runnable {
   *   public void run() {
   *     System.out.println("Read CUR_CLASS field: " + printGeneric(Transform1975.CUR_CLASS));
   *     System.out.println("Read REDEFINED_DEX_BYTES field: " + printGeneric(Transform1975.REDEFINED_DEX_BYTES));
   *     System.out.println("Read new_string field: " + printGeneric(Transform1975.new_string));
   *   }
   * }
   */
  private static final byte[] NEW_READ_BYTES =
      Base64.getDecoder()
          .decode(
              "ZGV4CjAzNQCtJUSLxM7nnY7/pRh8ApHzH+o9Ic0xb1qYBgAAcAAAAHhWNBIAAAAAAAAAANQFAAAk"
                  + "AAAAcAAAAA4AAAAAAQAABQAAADgBAAAEAAAAdAEAAAgAAACUAQAAAQAAANQBAACkBAAA9AEAAO4C"
                  + "AAD2AgAAAQMAAAQDAAAIAwAALAMAADwDAABRAwAAdQMAAJUDAACsAwAAvwMAANMDAADpAwAA/QMA"
                  + "ABgEAAAsBAAAQQQAAFkEAAB7BAAAlAQAAKkEAAC4BAAAuwQAAL8EAADDBAAA0AQAANgEAADeBAAA"
                  + "6gQAAO8EAAD9BAAABgUAAAsFAAAVBQAAHAUAAAQAAAAFAAAABgAAAAcAAAAIAAAACQAAAAoAAAAL"
                  + "AAAADAAAAA0AAAAOAAAADwAAABYAAAAYAAAAAgAAAAkAAAAAAAAAAwAAAAkAAADgAgAAAwAAAAoA"
                  + "AADoAgAAFgAAAAwAAAAAAAAAFwAAAAwAAADoAgAAAgAGAAEAAAACAA0AEAAAAAIACQAcAAAACwAF"
                  + "AB0AAAAAAAMAAAAAAAAAAwAgAAAAAQABAB4AAAAFAAQAHwAAAAcAAwAAAAAACgADAAAAAAAKAAIA"
                  + "GgAAAAoAAAAhAAAAAAAAABEAAAAHAAAA2AIAABUAAADEBQAAowUAAAAAAAABAAEAAQAAAMYCAAAE"
                  + "AAAAcBAEAAAADgAFAAEAAgAAAMoCAABVAAAAYgADAGIBAABxEAIAAQAMASICCgBwEAUAAgAaAxEA"
                  + "biAGADIAbiAGABIAbhAHAAIADAFuIAMAEABiAAMAYgEBAHEQAgABAAwBIgIKAHAQBQACABoDEgBu"
                  + "IAYAMgBuIAYAEgBuEAcAAgAMAW4gAwAQAGIAAwBiAQIAcRACAAEADAEiAgoAcBAFAAIAGgMTAG4g"
                  + "BgAyAG4gBgASAG4QBwACAAwBbiADABAADgAEAA4ABgAOARwPARwPARwPAAABAAAACAAAAAEAAAAH"
                  + "AAAAAQAAAAkABjxpbml0PgAJQ1VSX0NMQVNTAAFMAAJMTAAiTGFydC9UZXN0MTk3NSRSZWFkVHJh"
                  + "bnNmb3JtRmllbGRzOwAOTGFydC9UZXN0MTk3NTsAE0xhcnQvVHJhbnNmb3JtMTk3NTsAIkxkYWx2"
                  + "aWsvYW5ub3RhdGlvbi9FbmNsb3NpbmdDbGFzczsAHkxkYWx2aWsvYW5ub3RhdGlvbi9Jbm5lckNs"
                  + "YXNzOwAVTGphdmEvaW8vUHJpbnRTdHJlYW07ABFMamF2YS9sYW5nL0NsYXNzOwASTGphdmEvbGFu"
                  + "Zy9PYmplY3Q7ABRMamF2YS9sYW5nL1J1bm5hYmxlOwASTGphdmEvbGFuZy9TdHJpbmc7ABlMamF2"
                  + "YS9sYW5nL1N0cmluZ0J1aWxkZXI7ABJMamF2YS9sYW5nL1N5c3RlbTsAE1JFREVGSU5FRF9ERVhf"
                  + "QllURVMAFlJlYWQgQ1VSX0NMQVNTIGZpZWxkOiAAIFJlYWQgUkVERUZJTkVEX0RFWF9CWVRFUyBm"
                  + "aWVsZDogABdSZWFkIG5ld19zdHJpbmcgZmllbGQ6IAATUmVhZFRyYW5zZm9ybUZpZWxkcwANVGVz"
                  + "dDE5NzUuamF2YQABVgACVkwAAltCAAthY2Nlc3NGbGFncwAGYXBwZW5kAARuYW1lAApuZXdfc3Ry"
                  + "aW5nAANvdXQADHByaW50R2VuZXJpYwAHcHJpbnRsbgADcnVuAAh0b1N0cmluZwAFdmFsdWUAdn5+"
                  + "RDh7ImNvbXBpbGF0aW9uLW1vZGUiOiJkZWJ1ZyIsIm1pbi1hcGkiOjEsInNoYS0xIjoiYTgzNTJm"
                  + "MjU0ODg1MzYyY2NkOGQ5MDlkMzUyOWM2MDA5NGRkODk2ZSIsInZlcnNpb24iOiIxLjYuMjAtZGV2"
                  + "In0AAgMBIhgBAgQCGQQZGxcUAAABAQCBgAT0AwEBjAQAAAAAAAAAAgAAAJQFAACaBQAAuAUAAAAA"
                  + "AAAAAAAAAAAAABAAAAAAAAAAAQAAAAAAAAABAAAAJAAAAHAAAAACAAAADgAAAAABAAADAAAABQAA"
                  + "ADgBAAAEAAAABAAAAHQBAAAFAAAACAAAAJQBAAAGAAAAAQAAANQBAAABIAAAAgAAAPQBAAADIAAA"
                  + "AgAAAMYCAAABEAAAAwAAANgCAAACIAAAJAAAAO4CAAAEIAAAAgAAAJQFAAAAIAAAAQAAAKMFAAAD"
                  + "EAAAAgAAALQFAAAGIAAAAQAAAMQFAAAAEAAAAQAAANQFAAA=");

  static void ReadFields() throws Exception {
    Runnable r = new ReadTransformFields();
    System.out.println("Reading with reflection.");
    for (Field f : Transform1975.class.getFields()) {
      System.out.println(f.toString() + " = " + printGeneric(f.get(null)));
    }
    System.out.println("Reading normally in same class.");
    Transform1975.readFields();
    System.out.println("Reading with native.");
    readNativeFields(Transform1975.class, getNativeFields(Transform1975.class.getFields()));
    System.out.println("Reading normally in other class.");
    r.run();
    System.out.println("Doing modification maybe");
    Transform1975.doSomething();
    System.out.println("Reading with reflection after possible modification.");
    for (Field f : Transform1975.class.getFields()) {
      System.out.println(f.toString() + " = " + printGeneric(f.get(null)));
    }
    System.out.println("Reading normally in same class after possible modification.");
    Transform1975.readFields();
    System.out.println("Reading with native after possible modification.");
    readNativeFields(Transform1975.class, getNativeFields(Transform1975.class.getFields()));
    System.out.println("Reading normally in other class after possible modification.");
    r.run();
  }

  public static void doTest() throws Exception {
    Field[] old_fields = Transform1975.class.getFields();
    long[] old_native_fields = getNativeFields(Transform1975.class.getFields());
    ReadFields();
    Redefinition.doCommonStructuralClassRedefinition(
        Transform1975.class, Transform1975.REDEFINED_DEX_BYTES);
    Redefinition.doCommonClassRedefinition(
        ReadTransformFields.class, new byte[] {}, NEW_READ_BYTES);
    ReadFields();
    System.out.println("reading reflectively with old reflection objects");
    for (Field f : old_fields) {
      System.out.println("OLD FIELD OBJECT: " + f.toString() + " = " + printGeneric(f.get(null)));
    }
    System.out.println("reading natively with old jfieldIDs");
    readNativeFields(Transform1975.class, old_native_fields);
    System.out.println("reading natively with new jfieldIDs");
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
        "Field" + (PRINT_NONDETERMINISTIC ? " " + id : "") + " " + f + " = " + printGeneric(value));
  }

  public static native long[] getNativeFields(Field[] fields);

  public static native void readNativeFields(Class<?> field_class, long[] sfields);
}
