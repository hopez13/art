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

import java.util.function.Consumer;
import static test_919_transforms.Transform919.DEX_BYTES;
import static test_919_transforms.Transform919.CLASS_BYTES;

public class Test919 {
  // A class that we can use to keep track of the output of this test.
  private static class TestWatcher implements Consumer<String> {
    private StringBuilder sb;
    public TestWatcher() {
      sb = new StringBuilder();
    }

    @Override
    public void accept(String s) {
      sb.append(s);
      sb.append('\n');
    }

    public String getOutput() {
      return sb.toString();
    }
  }

  public static void run() {
    Redefinition.setTestConfiguration(Redefinition.Config.COMMON_REDEFINE);
    TestWatcher w = new TestWatcher();
    doTest(new Transform919(w), w);
  }

  public static void doTest(Transform919 t, TestWatcher w) {
    Runnable do_redefinition = () -> {
      w.accept("transforming calling function");
      Redefinition.doCommonClassRedefinition(Transform919.class, CLASS_BYTES, DEX_BYTES);
    };
    // This just prints something out to show we are running the Runnable.
    Runnable say_nothing = () -> { w.accept("Not doing anything here"); };

    // Try and redefine.
    t.sayHi(say_nothing);
    t.sayHi(do_redefinition);
    t.sayHi(say_nothing);

    // Print output of last run.
    System.out.print(w.getOutput());
  }
}
