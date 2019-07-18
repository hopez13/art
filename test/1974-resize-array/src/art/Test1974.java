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

import java.util.Arrays;
import java.util.function.Supplier;
public class Test1974 {

  public static int[] static_field = new int[] {1, 2, 3};
  public static Object[] static_field_ref = new String[] {"a", "b", "c"};
  public static final class InstanceClass {
    public int[] instance_field = new int[] {1, 2, 3};
    public Object[] self_ref;
    public InstanceClass() {
      self_ref = new Object[1];
      self_ref[0] = self_ref;
    }
  }

  static InstanceClass theInstanceClass;
  static InstanceClass theOtherInstanceClass;

  static {
    theInstanceClass = new InstanceClass();
    theOtherInstanceClass = new InstanceClass();
    theOtherInstanceClass.instance_field = theInstanceClass.instance_field;
    theOtherInstanceClass.self_ref = theInstanceClass.self_ref;
  }

  public static void run() throws Exception {
    System.out.println("Test instance");
    System.out.println("Pre hash: " + theInstanceClass.instance_field.hashCode());
    System.out.println("val is: " + Arrays.toString(theInstanceClass.instance_field) + " resize +3");
    ResizeArray(() -> theInstanceClass.instance_field, theInstanceClass.instance_field.length + 5);
    System.out.println("val is: " + Arrays.toString(theInstanceClass.instance_field));
    System.out.println("Post hash: " + theInstanceClass.instance_field.hashCode());
    System.out.println("Same value? " + (theInstanceClass.instance_field == theOtherInstanceClass.instance_field));

    System.out.println("Test instance self-ref");
    System.out.println("Pre hash: " + Integer.toHexString(theInstanceClass.self_ref.hashCode()));
    System.out.println("val is: " + Arrays.toString(theInstanceClass.self_ref) + " resize +5 item 0 is " + Arrays.toString((Object[])theInstanceClass.self_ref[0]));
    ResizeArray(() -> theInstanceClass.self_ref, theInstanceClass.self_ref.length + 5);
    System.out.println("val is: " + Arrays.toString(theInstanceClass.self_ref));
    System.out.println("val is: " + Arrays.toString((Object[])theInstanceClass.self_ref[0]));
    System.out.println("Post hash: " + Integer.toHexString(theInstanceClass.self_ref.hashCode()));
    System.out.println("Same value? " + (theInstanceClass.self_ref == theOtherInstanceClass.self_ref));
    System.out.println("Same structure? " + (theInstanceClass.self_ref == theInstanceClass.self_ref[0]));
    System.out.println("Same inner-structure? " + (theInstanceClass.self_ref[0] == ((Object[])theInstanceClass.self_ref[0])[0]));

    Thread t = new Thread(()-> {
      final int[] arr_loc = new int[] {2, 3, 4};
      int[] arr_loc_2 = arr_loc;

      System.out.println("Test local");
      System.out.println("Pre hash: " + arr_loc.hashCode());
      System.out.println("val is: " + Arrays.toString(arr_loc) + " resize +5");
      ResizeArray(() -> arr_loc, arr_loc.length + 5);
      System.out.println("val is: " + Arrays.toString(arr_loc));
      System.out.println("Post hash: " + arr_loc.hashCode());
      System.out.println("Same value? " + (arr_loc == arr_loc_2));
    });
    t.start();
    t.join();
  }

  // Use a supplier so that we don't have to have a local ref to the resized
  // array if we don't want it
  public static native void ResizeArray(Supplier<Object> arr, int new_size);
}
