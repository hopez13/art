/*
 * Copyright (C) 2015 The Android Open Source Project
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

import com.android.ahat.heapdump.AhatClassObj;
import com.android.ahat.heapdump.AhatHeap;
import com.android.ahat.heapdump.AhatInstance;
import com.android.ahat.heapdump.AhatSnapshot;
import com.android.ahat.heapdump.PathElement;
import com.android.ahat.heapdump.Value;

import java.io.IOException;
import java.util.List;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import org.junit.Test;

public class InstanceTest {
  @Test
  public void asStringBasic() throws IOException {
    TestDump dump = TestDump.getTestDump();
    AhatInstance str = dump.getDumpedAhatInstance("basicString");
    assertEquals("hello, world", str.asString());
  }

  @Test
  public void asStringCharArray() throws IOException {
    TestDump dump = TestDump.getTestDump();
    AhatInstance str = dump.getDumpedAhatInstance("charArray");
    assertEquals("char thing", str.asString());
  }

  @Test
  public void asStringTruncated() throws IOException {
    TestDump dump = TestDump.getTestDump();
    AhatInstance str = dump.getDumpedAhatInstance("basicString");
    assertEquals("hello", str.asString(5));
  }

  @Test
  public void asStringCharArrayTruncated() throws IOException {
    TestDump dump = TestDump.getTestDump();
    AhatInstance str = dump.getDumpedAhatInstance("charArray");
    assertEquals("char ", str.asString(5));
  }

  @Test
  public void asStringExactMax() throws IOException {
    TestDump dump = TestDump.getTestDump();
    AhatInstance str = dump.getDumpedAhatInstance("basicString");
    assertEquals("hello, world", str.asString(12));
  }

  @Test
  public void asStringCharArrayExactMax() throws IOException {
    TestDump dump = TestDump.getTestDump();
    AhatInstance str = dump.getDumpedAhatInstance("charArray");
    assertEquals("char thing", str.asString(10));
  }

  @Test
  public void asStringNotTruncated() throws IOException {
    TestDump dump = TestDump.getTestDump();
    AhatInstance str = dump.getDumpedAhatInstance("basicString");
    assertEquals("hello, world", str.asString(50));
  }

  @Test
  public void asStringCharArrayNotTruncated() throws IOException {
    TestDump dump = TestDump.getTestDump();
    AhatInstance str = dump.getDumpedAhatInstance("charArray");
    assertEquals("char thing", str.asString(50));
  }

  @Test
  public void asStringNegativeMax() throws IOException {
    TestDump dump = TestDump.getTestDump();
    AhatInstance str = dump.getDumpedAhatInstance("basicString");
    assertEquals("hello, world", str.asString(-3));
  }

  @Test
  public void asStringCharArrayNegativeMax() throws IOException {
    TestDump dump = TestDump.getTestDump();
    AhatInstance str = dump.getDumpedAhatInstance("charArray");
    assertEquals("char thing", str.asString(-3));
  }

  @Test
  public void asStringNull() throws IOException {
    TestDump dump = TestDump.getTestDump();
    AhatInstance obj = dump.getDumpedAhatInstance("nullString");
    assertNull(obj);
  }

  @Test
  public void asStringNotString() throws IOException {
    TestDump dump = TestDump.getTestDump();
    AhatInstance obj = dump.getDumpedAhatInstance("anObject");
    assertNotNull(obj);
    assertNull(obj.asString());
  }

  @Test
  public void basicReference() throws IOException {
    TestDump dump = TestDump.getTestDump();

    AhatInstance pref = dump.getDumpedAhatInstance("aPhantomReference");
    AhatInstance wref = dump.getDumpedAhatInstance("aWeakReference");
    AhatInstance nref = dump.getDumpedAhatInstance("aNullReferentReference");
    AhatInstance referent = dump.getDumpedAhatInstance("anObject");
    assertNotNull(pref);
    assertNotNull(wref);
    assertNotNull(nref);
    assertNotNull(referent);
    assertEquals(referent, pref.getReferent());
    assertEquals(referent, wref.getReferent());
    assertNull(nref.getReferent());
    assertNull(referent.getReferent());
  }

  @Test
  public void gcRootPath() throws IOException {
    TestDump dump = TestDump.getTestDump();

    AhatClassObj main = dump.getAhatSnapshot().findClass("Main");
    AhatInstance gcPathArray = dump.getDumpedAhatInstance("gcPathArray");
    Value value = gcPathArray.asArrayInstance().getValue(2);
    AhatInstance base = value.asAhatInstance();
    AhatInstance left = base.asClassInstance().getRefField("left");
    AhatInstance right = base.asClassInstance().getRefField("right");
    AhatInstance target = left.asClassInstance().getRefField("right");

    List<PathElement> path = target.getPathFromGcRoot();
    assertEquals(6, path.size());

    assertEquals(main, path.get(0).instance);
    assertEquals(".stuff", path.get(0).field);
    assertTrue(path.get(0).isDominator);

    assertEquals(".gcPathArray", path.get(1).field);
    assertTrue(path.get(1).isDominator);

    assertEquals(gcPathArray, path.get(2).instance);
    assertEquals("[2]", path.get(2).field);
    assertTrue(path.get(2).isDominator);

    assertEquals(base, path.get(3).instance);
    assertTrue(path.get(3).isDominator);

    // There are two possible paths. Either it can go through the 'left' node,
    // or the 'right' node.
    if (path.get(3).field.equals(".left")) {
      assertEquals(".left", path.get(3).field);

      assertEquals(left, path.get(4).instance);
      assertEquals(".right", path.get(4).field);
      assertFalse(path.get(4).isDominator);

    } else {
      assertEquals(".right", path.get(3).field);

      assertEquals(right, path.get(4).instance);
      assertEquals(".left", path.get(4).field);
      assertFalse(path.get(4).isDominator);
    }

    assertEquals(target, path.get(5).instance);
    assertEquals("", path.get(5).field);
    assertTrue(path.get(5).isDominator);
  }

  @Test
  public void retainedSize() throws IOException {
    TestDump dump = TestDump.getTestDump();

    // anObject should not be an immediate dominator of any other object. This
    // means its retained size should be equal to its size for the heap it was
    // allocated on, and should be 0 for all other heaps.
    AhatInstance anObject = dump.getDumpedAhatInstance("anObject");
    AhatSnapshot snapshot = dump.getAhatSnapshot();
    long size = anObject.getSize();
    assertEquals(size, anObject.getTotalRetainedSize());
    assertEquals(size, anObject.getRetainedSize(anObject.getHeap()));
    for (AhatHeap heap : snapshot.getHeaps()) {
      if (!heap.equals(anObject.getHeap())) {
        assertEquals(String.format("For heap '%s'", heap.getName()),
            0, anObject.getRetainedSize(heap));
      }
    }
  }

  @Test
  public void objectNotABitmap() throws IOException {
    TestDump dump = TestDump.getTestDump();
    AhatInstance obj = dump.getDumpedAhatInstance("anObject");
    assertNull(obj.asBitmap());
  }

  @Test
  public void arrayNotABitmap() throws IOException {
    TestDump dump = TestDump.getTestDump();
    AhatInstance obj = dump.getDumpedAhatInstance("gcPathArray");
    assertNull(obj.asBitmap());
  }

  @Test
  public void classObjNotABitmap() throws IOException {
    TestDump dump = TestDump.getTestDump();
    AhatInstance obj = dump.getAhatSnapshot().findClass("Main");
    assertNull(obj.asBitmap());
  }

  @Test
  public void classInstanceToString() throws IOException {
    TestDump dump = TestDump.getTestDump();
    AhatInstance obj = dump.getDumpedAhatInstance("aPhantomReference");
    long id = obj.getId();
    assertEquals(String.format("java.lang.ref.PhantomReference@%08x", id), obj.toString());
  }

  @Test
  public void classObjToString() throws IOException {
    TestDump dump = TestDump.getTestDump();
    AhatInstance obj = dump.getAhatSnapshot().findClass("Main");
    assertEquals("Main", obj.toString());
  }

  @Test
  public void arrayInstanceToString() throws IOException {
    TestDump dump = TestDump.getTestDump();
    AhatInstance obj = dump.getDumpedAhatInstance("gcPathArray");
    long id = obj.getId();
    assertEquals(String.format("Main$ObjectTree[4]@%08x", id), obj.toString());
  }

  @Test
  public void primArrayInstanceToString() throws IOException {
    TestDump dump = TestDump.getTestDump();
    AhatInstance obj = dump.getDumpedAhatInstance("bigArray");
    long id = obj.getId();
    assertEquals(String.format("byte[1000000]@%08x", id), obj.toString());
  }

  @Test
  public void isNotRoot() throws IOException {
    TestDump dump = TestDump.getTestDump();
    AhatInstance obj = dump.getDumpedAhatInstance("anObject");
    assertFalse(obj.isRoot());
    assertNull(obj.getRootTypes());
  }
}
