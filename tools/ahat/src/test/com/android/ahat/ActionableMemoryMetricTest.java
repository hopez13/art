/*
 * Copyright (C) 2018 The Android Open Source Project
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
import com.android.ahat.heapdump.ExternalModelSource;
import java.io.IOException;
import java.util.ArrayList;
import java.util.List;
import org.junit.Test;

import static org.junit.Assert.assertEquals;

public class ActionableMemoryMetricTest {

  private static TestDump getAMMTestDump() throws IOException {
    return TestDump.getTestDump("AmmTest.hprof",
                                null,
                                null,
                                ExternalModelSource.ACTIONABLE_MEMORY_METRIC);
  }

  @Test
  public void doesNotCrash() throws IOException {
    getAMMTestDump();
  }

  @Test
  public void bitmapModel() throws IOException {
    TestDump dump = getAMMTestDump();
    AhatSnapshot snapshot = dump.getAhatSnapshot();
    List<AhatInstance> mainActivities = new ArrayList<AhatInstance>();
    snapshot.getRootSite().getObjects(null, "com.android.amm.test.MainActivity", mainActivities);
    assertEquals(1, mainActivities.size());
    AhatInstance main = mainActivities.get(0);
    AhatInstance bitmap = main.getRefField("mBitmapUse").getRefField("mBitmap");
    assertEquals(4 * 132 * 154, bitmap.getSize().getModeledExternalSize());
  }
}
