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

import dalvik.system.InMemoryDexClassLoader;
import java.nio.ByteBuffer;
import java.util.ArrayList;
import java.util.Base64;
import java.util.concurrent.CountDownLatch;
import java.util.function.Supplier;
import java.util.concurrent.atomic.*;
import java.lang.ref.*;

public class Test2006 {
  public static final CountDownLatch start_latch = new CountDownLatch(1);
  public static final CountDownLatch redefine_latch = new CountDownLatch(1);
  public static final CountDownLatch finish_latch = new CountDownLatch(1);
  public static volatile int start_counter = 0;
  public static volatile int finish_counter = 0;
  public static class Transform {
    public Transform() { }
    protected void finalize() throws Throwable {
      System.out.println("Finalizing");
      start_counter++;
      start_latch.countDown();
      redefine_latch.await();
      finish_counter++;
      finish_latch.countDown();
    }
  }

  /**
   * base64 encoded class/dex file for
   * public static class Transform {
   *   public String greeting;
   *
   *   public Transform() {
   *     greeting = "Hello";
   *   }
   *   protected void finalize() {
   *     // Don't do anything
   *   }
   * }
   */
  private static final byte[] DEX_BYTES =
      Base64.getDecoder()
          .decode(
"ZGV4CjAzNQC1pMXn/Bx3Ovsmr5bDHQ2ZVSMEHkVt6sLQAwAAcAAAAHhWNBIAAAAAAAAAABgDAAAR" +
"AAAAcAAAAAcAAAC0AAAAAQAAANAAAAABAAAA3AAAAAMAAADkAAAAAQAAAPwAAAC0AgAAHAEAAFgB" +
"AABgAQAAZwEAAIEBAACRAQAAtQEAANUBAADpAQAA/QEAAAwCAAAXAgAAGgIAACcCAAAxAgAAOwIA" +
"AEECAABIAgAAAgAAAAMAAAAEAAAABQAAAAYAAAAHAAAACgAAAAoAAAAGAAAAAAAAAAAABQANAAAA" +
"AAAAAAAAAAAAAAAADAAAAAQAAAAAAAAAAAAAAAEAAAAEAAAAAAAAAAgAAAAIAwAA5gIAAAAAAAAC" +
"AAEAAQAAAE4BAAAIAAAAcBACAAEAGgABAFsQAAAOAAEAAQAAAAAAVAEAAAEAAAAOAAkADjxLAA4A" +
"DgAGPGluaXQ+AAVIZWxsbwAYTGFydC9UZXN0MjAwNiRUcmFuc2Zvcm07AA5MYXJ0L1Rlc3QyMDA2" +
"OwAiTGRhbHZpay9hbm5vdGF0aW9uL0VuY2xvc2luZ0NsYXNzOwAeTGRhbHZpay9hbm5vdGF0aW9u" +
"L0lubmVyQ2xhc3M7ABJMamF2YS9sYW5nL09iamVjdDsAEkxqYXZhL2xhbmcvU3RyaW5nOwANVGVz" +
"dDIwMDYuamF2YQAJVHJhbnNmb3JtAAFWAAthY2Nlc3NGbGFncwAIZmluYWxpemUACGdyZWV0aW5n" +
"AARuYW1lAAV2YWx1ZQCMAX5+RDh7ImNvbXBpbGF0aW9uLW1vZGUiOiJkZWJ1ZyIsImhhcy1jaGVj" +
"a3N1bXMiOmZhbHNlLCJtaW4tYXBpIjoxLCJzaGEtMSI6IjEyOWVlOWY2NzU2YzM5ZGY1N2ZmMDc4" +
"NWQyNTZiMjM3NzI2NDJiN2MiLCJ2ZXJzaW9uIjoiMi4wLjEwLWRldiJ9AAICAQ8YAQIDAgsECQ4X" +
"CQABAQEAAQCBgAScAgEEvAIAAAAAAAACAAAA1wIAAN0CAAD8AgAAAAAAAAAAAAAAAAAADwAAAAAA" +
"AAABAAAAAAAAAAEAAAARAAAAcAAAAAIAAAAHAAAAtAAAAAMAAAABAAAA0AAAAAQAAAABAAAA3AAA" +
"AAUAAAADAAAA5AAAAAYAAAABAAAA/AAAAAEgAAACAAAAHAEAAAMgAAACAAAATgEAAAIgAAARAAAA" +
"WAEAAAQgAAACAAAA1wIAAAAgAAABAAAA5gIAAAMQAAACAAAA+AIAAAYgAAABAAAACAMAAAAQAAAB" +
"AAAAGAMAAA==");

  public static void run() throws Exception {
    Redefinition.setTestConfiguration(Redefinition.Config.COMMON_REDEFINE);
    doTest();
  }

  public static void doTest() throws Exception {
    Thread gc_thr = new Thread(() -> {
      while (true) {
        System.gc();
      }
    });
    gc_thr.setDaemon(true);
    gc_thr.start();
    mktransform();
    Thread eater = new Thread(() -> {
      ArrayList<Object> arr = new ArrayList<>();
      while (true) {
        try {
          arr.add(new Object());
        } catch (Exception e) {
          arr.clear();
        }
      }
    });
    eater.setDaemon(true);
    eater.start();
    start_latch.await();
    System.out.println("start_counter: " + start_counter);
    Redefinition.doCommonStructuralClassRedefinition(Transform.class, DEX_BYTES);
    redefine_latch.countDown();
    finish_latch.await();
    System.out.println("Finish_counter: " + finish_counter);
  }
  public static void mktransform() throws Exception {
    Transform tr = new Transform();
  }
}
