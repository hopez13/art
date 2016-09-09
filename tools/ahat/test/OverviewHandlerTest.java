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

package com.android.ahat;

import com.android.ahat.heapdump.AhatSnapshot;

import java.io.File;
import java.io.IOException;
import java.io.OutputStream;
import java.io.PrintStream;
import org.junit.Test;

public class OverviewHandlerTest {
  private static class NullOutputStream extends OutputStream {
    public void write(int b) throws IOException {
    }
  }

  @Test
  public void noCrash() throws IOException {
    TestDump dump = TestDump.getTestDump();

    // The overview view should not crash.
    AhatSnapshot snapshot = dump.getAhatSnapshot();
    AhatHandler handler = new OverviewHandler(snapshot, new File("my.hprof.file"));

    PrintStream ps = new PrintStream(new NullOutputStream());
    HtmlDoc doc = new HtmlDoc(ps, DocString.text("noCrash test"), DocString.uri("style.css"));
    String uri = "http://localhost:7100";
    Query query = new Query(DocString.uri(uri));
    handler.handle(doc, query);
  }
}
