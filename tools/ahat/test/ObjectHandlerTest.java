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

import com.android.ahat.heapdump.AhatInstance;
import com.android.ahat.heapdump.AhatSnapshot;

import java.io.IOException;
import java.io.OutputStream;
import java.io.PrintStream;
import static org.junit.Assert.assertNotNull;
import org.junit.Test;

public class ObjectHandlerTest {
  private static class NullOutputStream extends OutputStream {
    public void write(int b) throws IOException {
    }
  }

  @Test
  public void noCrashClassInstance() throws IOException {
    TestDump dump = TestDump.getTestDump();

    AhatInstance object = dump.getDumpedAhatInstance("aPhantomReference");
    assertNotNull(object);

    // The objects view should not crash for a class instance.
    AhatSnapshot snapshot = dump.getAhatSnapshot();
    AhatHandler handler = new ObjectHandler(snapshot);

    PrintStream ps = new PrintStream(new NullOutputStream());
    HtmlDoc doc = new HtmlDoc(ps, DocString.text("noCrash test"), DocString.uri("style.css"));
    String uri = "http://localhost:7100/object?id=" + object.getId();
    Query query = new Query(DocString.uri(uri));
    handler.handle(doc, query);
  }

  @Test
  public void noCrashClassObj() throws IOException {
    TestDump dump = TestDump.getTestDump();


    // The objects view should not crash for a class instance.
    AhatSnapshot snapshot = dump.getAhatSnapshot();
    AhatHandler handler = new ObjectHandler(snapshot);

    AhatInstance object = snapshot.findClass("Main");
    assertNotNull(object);

    PrintStream ps = new PrintStream(new NullOutputStream());
    HtmlDoc doc = new HtmlDoc(ps, DocString.text("noCrash test"), DocString.uri("style.css"));
    String uri = "http://localhost:7100/object?id=" + object.getId();
    Query query = new Query(DocString.uri(uri));
    handler.handle(doc, query);
  }

  @Test
  public void noCrashSystemClassObj() throws IOException {
    TestDump dump = TestDump.getTestDump();


    // The objects view should not crash for a class instance.
    AhatSnapshot snapshot = dump.getAhatSnapshot();
    AhatHandler handler = new ObjectHandler(snapshot);

    AhatInstance object = snapshot.findClass("java.lang.String");
    assertNotNull(object);

    PrintStream ps = new PrintStream(new NullOutputStream());
    HtmlDoc doc = new HtmlDoc(ps, DocString.text("noCrash test"), DocString.uri("style.css"));
    String uri = "http://localhost:7100/object?id=" + object.getId();
    Query query = new Query(DocString.uri(uri));
    handler.handle(doc, query);
  }

  @Test
  public void noCrashArrayInstance() throws IOException {
    TestDump dump = TestDump.getTestDump();

    AhatInstance object = dump.getDumpedAhatInstance("gcPathArray");
    assertNotNull(object);

    // The objects view should not crash for a class instance.
    AhatSnapshot snapshot = dump.getAhatSnapshot();
    AhatHandler handler = new ObjectHandler(snapshot);

    PrintStream ps = new PrintStream(new NullOutputStream());
    HtmlDoc doc = new HtmlDoc(ps, DocString.text("noCrash test"), DocString.uri("style.css"));
    String uri = "http://localhost:7100/object?id=" + object.getId();
    Query query = new Query(DocString.uri(uri));
    handler.handle(doc, query);
  }
}
