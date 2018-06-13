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

public class Main {

  boolean b00;
  boolean b01;
  boolean b02;
  boolean b03;
  boolean b04;
  boolean b05;
  boolean b06;
  boolean b07;
  boolean b08;
  boolean b09;
  boolean b10;
  boolean b11;
  boolean b12;
  boolean b13;
  boolean b14;
  boolean b15;
  boolean b16;
  boolean b17;
  boolean b18;
  boolean b19;
  boolean b20;
  boolean b21;
  boolean b22;
  boolean b23;
  boolean b24;
  boolean b25;
  boolean b26;
  boolean b27;
  boolean b28;
  boolean b29;
  boolean b30;
  boolean b31;
  boolean b32;
  boolean b33;
  boolean b34;
  boolean b35;
  boolean b36;

  boolean conditionA;
  boolean conditionB;
  boolean conditionC;

  public static void main(String[] args) throws Exception {
    Class main2 = Class.forName("Main2");
    main2.getMethod("test").invoke(main2.newInstance());
    System.out.println("passed");
  }
}
