/*
 * Copyright (C) 2019 The Android Open Source Project
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

import java.util.Base64;
public class Transform1976 {

  static {}

  /* Dex file for:
   * package art;
   * public class Transform1975 {
   *  static {}
   *  public static Class<?> CUR_CLASS;
   *  public static byte[] REDEFINED_DEX_BYTES;
   *  public static String new_string;
   *  public static void doSomething() {
   *    System.out.println("Doing something");
   *    new_string = "I did something!";
   *  }
   *  public static void readFields() {
   *    System.out.println("NEW VALUE CUR_CLASS: " + CUR_CLASS);
   *    System.out.println("NEW VALUE REDEFINED_DEX_BYTES: " + Base64.getEncoder().encodeToString(REDEFINED_DEX_BYTES));
   *    System.out.println("NEW VALUE new_string: " + new_string);
   *  }
   * }
   */
  public static byte[] REDEFINED_DEX_BYTES = Base64.getDecoder().decode("");

  public static void doSomething() {
  }

  public static void readFields() {
  }
}