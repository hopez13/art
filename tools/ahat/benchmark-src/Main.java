/*
 * Copyright (C) 2017 The Android Open Source Project
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

import com.android.ahat.heapdump.Parser;
import com.android.ahat.proguard.ProguardMap;
import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.nio.ByteBuffer;

public class Main {
  private static ByteBuffer hprof;

  /**
   * Read the named resource into a ByteBuffer.
   */
  private static ByteBuffer dataBufferFromResource(String name) throws IOException {
    ClassLoader loader = Main.class.getClassLoader();
    InputStream is = loader.getResourceAsStream(name);
    ByteArrayOutputStream baos = new ByteArrayOutputStream();
    byte[] buf = new byte[4096];
    int read;
    while ((read = is.read(buf)) != -1) {
      baos.write(buf, 0, read);
    }
    return ByteBuffer.wrap(baos.toByteArray());
  }

  // Run the benchmark.
  private static void run() throws Exception {
    hprof.position(0);
    Parser.parseHeapDump(hprof, new ProguardMap());
  }

  public static void main(String[] args) throws Exception {
    hprof = dataBufferFromResource("test-dump.hprof");

    // Warmup run.
    run();

    // Run the benchmark for up to one minute.
    int count = 0;
    long elapsed = 0;
    while (elapsed < 60 * 1000) {
      Runtime.getRuntime().gc();
      long start = System.currentTimeMillis();
      run();
      elapsed += System.currentTimeMillis() - start;
      count++;
    }
    System.out.println("AhatBenchmark(RunTime): " + (elapsed / count) + " ms");
  }
}
