/*
 * Copyright (C) 2011 The Android Open Source Project
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
import java.lang.reflect.Method;
import java.util.List;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.function.IntUnaryOperator;

public class Test988 {

    static interface Printable {
        public void Print();
    }

    static final class MethodEntry implements Printable {
        private Object m;
        private int cnt;
        public MethodEntry(Object m, int cnt) {
            this.m = m;
            this.cnt = cnt;
        }
        @Override
        public void Print() {
            System.out.println(whitespace(cnt) + "=> " + m);
        }
    }

    static final class MethodReturn implements Printable {
        private Object m;
        private Object val;
        private int cnt;
        public MethodReturn(Object m, Object val, int cnt) {
            this.m = m;
            this.val = val;
            this.cnt = cnt;
        }
        @Override
        public void Print() {
            Object print = val;
            if (val != null && val.getClass().isArray()) {
                print = Arrays.toString((Object[])val);
            }
            System.out.println(whitespace(cnt) + "<= " + m + " -> " + print);
        }
    }

    static final class MethodThrownThrough implements Printable {
        private Object m;
        private int cnt;
        public MethodThrownThrough(Object m, int cnt) {
            this.m = m;
            this.cnt = cnt;
        }
        @Override
        public void Print() {
            System.out.println(whitespace(cnt) + "<= " + m + " EXCEPTION");
        }
    }

    private static String whitespace(int n) {
      String out = "";
      while (n > 0) {
        n--;
        out += ".";
      }
      return out;
    }

    static final class FibResult implements Printable {
        private String format;
        private int arg;
        private int res;
        public FibResult(String format, int arg, int res) {
            this.format = format;
            this.arg = arg;
            this.res = res;
        }

        @Override
        public void Print() {
            System.out.printf(format, arg, res);
        }
    }

    private static List<Printable> results = new ArrayList<>();
    private static int cnt = 1;

    // Memoizing version
    static final class MemoOp implements IntUnaryOperator {
      public int applyAsInt(int x) {
        return memo_fibonacci(x);
      }
    }
    private static HashMap<Integer, Integer> memo = new HashMap<>();
    static int memo_fibonacci(int n) {
      if (memo.containsKey(n)) {
        return memo.get(n);
      } else if (n == 0 || n == 1) {
        return n;
      }
      int res = memo_fibonacci(n - 1) + memo_fibonacci(n - 2);
      memo.put(n, res);
      return res;
    }

    // Iterative version
    static final class IterOp implements IntUnaryOperator {
      public int applyAsInt(int x) {
        return iter_fibonacci(x);
      }
    }
    static int iter_fibonacci(int n) {
        if (n == 0) {
            return 0;
        }
        int x = 1;
        int y = 1;
        for (int i = 3; i <= n; i++) {
            int z = x + y;
            x = y;
            y = z;
        }
        return y;
    }

    // Recursive version
    static final class RecurOp implements IntUnaryOperator {
      public int applyAsInt(int x) {
        return fibonacci(x);
      }
    }
    static int fibonacci(int n) {
        if ((n == 0) || (n == 1)) {
            return n;
        } else {
            return fibonacci(n - 1) + (fibonacci(n - 2));
        }
    }

    public static void notifyMethodEntry(Object m) {
        // Called by native code when a method is entered. This method is ignored by the native
        // entry and exit hooks.
        results.add(new MethodEntry(m, cnt));
        cnt++;
    }

    public static void notifyMethodExit(Object m, boolean exception, Object result) {
        cnt--;
        if (exception) {
            results.add(new MethodThrownThrough(m, cnt));
        } else {
            results.add(new MethodReturn(m, result, cnt));
        }
    }

    public static void run() throws Exception {
        // call this here so it is linked. It doesn't actually do anything here.
        loadAllClasses();
        disableMethodTracing();
        enableMethodTracing(Test988.class.getDeclaredMethod("notifyMethodEntry", Object.class),
                Test988.class.getDeclaredMethod("notifyMethodExit",
                  Object.class, Boolean.TYPE, Object.class));
        doTest(30, new IterOp());
        doTest(15, new MemoOp());
        doTest(5, new RecurOp());
        // Turn off method tracing so we don't have to deal with print internals.
        disableMethodTracing();
        printResults();
    }

    // This ensures that all classes we touch are loaded before we start recording traces. This
    // eliminates a major source of divergence between the RI and ART.
    public static void loadAllClasses() {
      MethodThrownThrough.class.toString();
      MethodEntry.class.toString();
      MethodReturn.class.toString();
      FibResult.class.toString();
      Printable.class.toString();
      ArrayList.class.toString();
      RecurOp.class.toString();
      MemoOp.class.toString();
      IterOp.class.toString();
      memo.put(0, 0);
      memo.put(1, 1);
    }

    public static void printResults() {
        for (Printable p : results) {
            p.Print();
        }
    }

    private static void pushResult(String format, int arg, int res) {
        results.add(new FibResult(format, arg, res));
    }

    public static void doTest(int x, IntUnaryOperator op) {
        int y = op.applyAsInt(x);
        pushResult("fibonacci(%d)=%d\n", x, y);
    }

    private static native void enableMethodTracing(Method entryMethod, Method exitMethod);
    private static native void disableMethodTracing();
}
