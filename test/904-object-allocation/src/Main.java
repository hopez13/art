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
  public static void main(String[] args) throws Exception {
    System.loadLibrary(args[1]);
    doTest();
  }

  public static void doTest() throws Exception {
    setupCallback();

    enableAllocationTracking(null, true);

    new Object();
    new Integer(1);

    enableAllocationTracking(null, false);

    new Float(1.0f);

    enableAllocationTracking(Thread.currentThread(), true);

    new Short((short)0);

    enableAllocationTracking(Thread.currentThread(), false);

    new Byte((byte)0);

    System.out.println("Tracking on same thread");

    testThread(true, true);

    new Byte((byte)0);

    System.out.println("Tracking on same thread, not disabling tracking");

    testThread(true, false);

    System.out.println("Tracking on different thread");

    testThread(false, true);

    new Byte((byte)0);
  }

  private static void testThread(final boolean sameThread, final boolean disableTracking)
      throws Exception {
    final SimpleBarrier startBarrier = new SimpleBarrier(1);
    final SimpleBarrier trackBarrier = new SimpleBarrier(1);
    final SimpleBarrier disableBarrier = new SimpleBarrier(1);

    Thread t = new Thread() {
      public void run() {
        try {
          startBarrier.dec();
          trackBarrier.waitFor();
        } catch (Exception e) {
          e.printStackTrace(System.out);
          System.exit(1);
        }

        new Double(0.0);

        if (disableTracking) {
          enableAllocationTracking(sameThread ? this : Thread.currentThread(), false);
        }
      }
    };

    t.start();
    startBarrier.waitFor();
    enableAllocationTracking(sameThread ? t : Thread.currentThread(), true);
    trackBarrier.dec();

    t.join();
  }

  private static class SimpleBarrier {
    int count;

    public SimpleBarrier(int i) {
      count = i;
    }

    public synchronized void dec() throws Exception {
      count--;
      notifyAll();
    }

    public synchronized void waitFor() throws Exception  {
      while (count != 0) {
        wait();
      }
    }
  }

  private static native void setupCallback();
  private static native void enableAllocationTracking(Thread thread, boolean enable);
}
