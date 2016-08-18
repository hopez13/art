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

  public static boolean doThrow = false;

  public void $noinline$foo(int in_w1,
                            int in_w2,
                            int in_w3,
                            int in_w4,
                            int in_w5,
                            int in_w6,
                            int in_w7,
                            int on_stack_int,
                            long on_stack_long,
                            float in_s0,
                            float in_s1,
                            float in_s2,
                            float in_s3,
                            float in_s4,
                            float in_s5,
                            float in_s6,
                            float in_s7,
                            float on_stack_float,
                            double on_stack_double) {
    if (doThrow) throw new Error();
  }

  // We expect a parallel move that moves four times the zero constant to stack locations.
  /// CHECK-START-ARM64: void Main.bar() register (after)
  /// CHECK:             ParallelMove {{.*#0->[0-9x]+\(sp\).*#0->[0-9x]+\(sp\).*#0->[0-9x]+\(sp\).*#0->[0-9x]+\(sp\).*}}

  // Those four moves should generate four 'store' instructions using directly the zero register.
  /// CHECK-START-ARM64: void Main.bar() disassembly (after)
  /// CHECK-DAG:         {{(str|stur)}} wzr, [sp, #{{[0-9]+}}]
  /// CHECK-DAG:         {{(str|stur)}} xzr, [sp, #{{[0-9]+}}]
  /// CHECK-DAG:         {{(str|stur)}} wzr, [sp, #{{[0-9]+}}]
  /// CHECK-DAG:         {{(str|stur)}} xzr, [sp, #{{[0-9]+}}]

  public void bar() {
    $noinline$foo(1, 2, 3, 4, 5, 6, 7,     // Integral values in registers.
                  0, 0L,                   // Integral values on the stack.
                  1, 2, 3, 4, 5, 6, 7, 8,  // Floating-point values in registers.
                  0.0f, 0.0);              // Floating-point values on the stack.
  }

  public static byte static_byte_field;
  public static char static_char_field;
  public static short static_short_field;
  public static int static_int_field;
  public static long static_long_field;
  public static float static_float_field;
  public static double static_double_field;
  public static volatile byte volatile_static_byte_field;
  public static volatile char volatile_static_char_field;
  public static volatile short volatile_static_short_field;
  public static volatile int volatile_static_int_field;
  public static volatile long volatile_static_long_field;
  public static volatile float volatile_static_float_field;
  public static volatile double volatile_static_double_field;

  /// CHECK-START-ARM64: void Main.store_zero_to_static_fields() disassembly (after)
  /// CHECK-DAG:         StaticFieldSet
  /// CHECK-DAG:         StaticFieldSet
  /// CHECK-DAG:         StaticFieldSet
  /// CHECK-DAG:         StaticFieldSet
  /// CHECK-DAG:         StaticFieldSet
  /// CHECK-DAG:         StaticFieldSet
  /// CHECK-DAG:         StaticFieldSet
  /// CHECK-DAG:         StaticFieldSet
  /// CHECK-DAG:         StaticFieldSet
  /// CHECK-DAG:         StaticFieldSet
  /// CHECK-DAG:         StaticFieldSet
  /// CHECK-DAG:         StaticFieldSet
  /// CHECK-DAG:         StaticFieldSet
  /// CHECK-DAG:         StaticFieldSet

  /// CHECK-START-ARM64: void Main.store_zero_to_static_fields() disassembly (after)
  /// CHECK-DAG:         strb wzr, [x{{[0-9]+}}, #{{[0-9]+}}]
  /// CHECK-DAG:         strh wzr, [x{{[0-9]+}}, #{{[0-9]+}}]
  /// CHECK-DAG:         strh wzr, [x{{[0-9]+}}, #{{[0-9]+}}]
  /// CHECK-DAG:         str wzr, [x{{[0-9]+}}, #{{[0-9]+}}]
  /// CHECK-DAG:         str xzr, [x{{[0-9]+}}, #{{[0-9]+}}]
  /// CHECK-DAG:         str wzr, [x{{[0-9]+}}, #{{[0-9]+}}]
  /// CHECK-DAG:         str xzr, [x{{[0-9]+}}, #{{[0-9]+}}]
  /// CHECK-DAG:         stlrb wzr, [x{{[0-9]+}}]
  /// CHECK-DAG:         stlrh wzr, [x{{[0-9]+}}]
  /// CHECK-DAG:         stlrh wzr, [x{{[0-9]+}}]
  /// CHECK-DAG:         stlr wzr, [x{{[0-9]+}}]
  /// CHECK-DAG:         stlr xzr, [x{{[0-9]+}}]
  /// CHECK-DAG:         stlr wzr, [x{{[0-9]+}}]
  /// CHECK-DAG:         stlr xzr, [x{{[0-9]+}}]

  public void store_zero_to_static_fields() {
    static_byte_field = 0;
    static_char_field = 0;
    static_short_field = 0;
    static_int_field = 0;
    static_long_field = 0;
    static_float_field = 0.0f;
    static_double_field = 0.0;
    volatile_static_byte_field = 0;
    volatile_static_char_field = 0;
    volatile_static_short_field = 0;
    volatile_static_int_field = 0;
    volatile_static_long_field = 0;
    volatile_static_float_field = 0.0f;
    volatile_static_double_field = 0.0;
  }

  public byte instance_byte_field;
  public char instance_char_field;
  public short instance_short_field;
  public int instance_int_field;
  public long instance_long_field;
  public float instance_float_field;
  public double instance_double_field;
  public volatile byte volatile_instance_byte_field;
  public volatile char volatile_instance_char_field;
  public volatile short volatile_instance_short_field;
  public volatile int volatile_instance_int_field;
  public volatile long volatile_instance_long_field;
  public volatile float volatile_instance_float_field;
  public volatile double volatile_instance_double_field;

  /// CHECK-START-ARM64: void Main.store_zero_to_instance_fields() register (after)
  /// CHECK-DAG:         InstanceFieldSet
  /// CHECK-DAG:         InstanceFieldSet
  /// CHECK-DAG:         InstanceFieldSet
  /// CHECK-DAG:         InstanceFieldSet
  /// CHECK-DAG:         InstanceFieldSet
  /// CHECK-DAG:         InstanceFieldSet
  /// CHECK-DAG:         InstanceFieldSet
  /// CHECK-DAG:         InstanceFieldSet
  /// CHECK-DAG:         InstanceFieldSet
  /// CHECK-DAG:         InstanceFieldSet
  /// CHECK-DAG:         InstanceFieldSet
  /// CHECK-DAG:         InstanceFieldSet
  /// CHECK-DAG:         InstanceFieldSet
  /// CHECK-DAG:         InstanceFieldSet

  /// CHECK-START-ARM64: void Main.store_zero_to_instance_fields() disassembly (after)
  /// CHECK-DAG:         strb wzr, [x{{[0-9]+}}, #{{[0-9]+}}]
  /// CHECK-DAG:         strh wzr, [x{{[0-9]+}}, #{{[0-9]+}}]
  /// CHECK-DAG:         strh wzr, [x{{[0-9]+}}, #{{[0-9]+}}]
  /// CHECK-DAG:         str wzr, [x{{[0-9]+}}, #{{[0-9]+}}]
  /// CHECK-DAG:         str xzr, [x{{[0-9]+}}, #{{[0-9]+}}]
  /// CHECK-DAG:         str wzr, [x{{[0-9]+}}, #{{[0-9]+}}]
  /// CHECK-DAG:         str xzr, [x{{[0-9]+}}, #{{[0-9]+}}]
  /// CHECK-DAG:         stlrb wzr, [x{{[0-9]+}}]
  /// CHECK-DAG:         stlrh wzr, [x{{[0-9]+}}]
  /// CHECK-DAG:         stlrh wzr, [x{{[0-9]+}}]
  /// CHECK-DAG:         stlr wzr, [x{{[0-9]+}}]
  /// CHECK-DAG:         stlr xzr, [x{{[0-9]+}}]
  /// CHECK-DAG:         stlr wzr, [x{{[0-9]+}}]
  /// CHECK-DAG:         stlr xzr, [x{{[0-9]+}}]

  public void store_zero_to_instance_fields() {
    instance_byte_field = 0;
    instance_char_field = 0;
    instance_short_field = 0;
    instance_int_field = 0;
    instance_long_field = 0;
    instance_float_field = 0.0f;
    instance_double_field = 0.0;
    volatile_instance_byte_field = 0;
    volatile_instance_char_field = 0;
    volatile_instance_short_field = 0;
    volatile_instance_int_field = 0;
    volatile_instance_long_field = 0;
    volatile_instance_float_field = 0.0f;
    volatile_instance_double_field = 0.0;
  }

  /// CHECK-START-ARM64: void Main.store_zero_to_arrays() register (after)
  /// CHECK-DAG:         ArraySet
  /// CHECK-DAG:         ArraySet
  /// CHECK-DAG:         ArraySet
  /// CHECK-DAG:         ArraySet
  /// CHECK-DAG:         ArraySet
  /// CHECK-DAG:         ArraySet
  /// CHECK-DAG:         ArraySet

  /// CHECK-START-ARM64: void Main.store_zero_to_arrays() disassembly (after)
  /// CHECK-DAG:         strb wzr, [x{{[0-9]+}}, #{{[0-9]+}}]
  /// CHECK-DAG:         strh wzr, [x{{[0-9]+}}, #{{[0-9]+}}]
  /// CHECK-DAG:         strh wzr, [x{{[0-9]+}}, #{{[0-9]+}}]
  /// CHECK-DAG:         str wzr, [x{{[0-9]+}}, #{{[0-9]+}}]
  /// CHECK-DAG:         str xzr, [x{{[0-9]+}}, #{{[0-9]+}}]
  /// CHECK-DAG:         str wzr, [x{{[0-9]+}}, #{{[0-9]+}}]
  /// CHECK-DAG:         str xzr, [x{{[0-9]+}}, #{{[0-9]+}}]

  byte array_byte[];
  char array_char[];
  short array_short[];
  int array_int[];
  long array_long[];
  float array_float[];
  double array_double[];

  public void store_zero_to_arrays() {
    array_byte[0] = 0;
    array_char[0] = 0;
    array_short[0] = 0;
    array_int[0] = 0;
    array_long[0] = 0;
    array_float[0] = 0.0f;
    array_double[0] = 0.0;
  }

  public static void main(String args[]) {}
}
