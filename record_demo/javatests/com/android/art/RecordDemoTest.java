/*
 * Copyright (C) 2024 The Android Open Source Project
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
 * limitations under the License
 */

package com.android.art;

import static com.google.common.truth.Truth.assertThat;

import androidx.test.filters.SmallTest;
import androidx.test.runner.AndroidJUnit4;

import com.google.auto.value.AutoValue;

import org.junit.Test;
import org.junit.runner.RunWith;

import java.util.HashMap;

@SmallTest
@RunWith(AndroidJUnit4.class)
public class RecordDemoTest {
    @Test
    public void testRecord() {
        var map = new HashMap<RecordKey, Integer>();
        map.put(new RecordKey("a", 1), 1);

        for (int i = 0; i < 1000000; i++) {
            assertThat(map.get(new RecordKey("a", 1))).isEqualTo(1);
        }
    }

    @Test
    public void testAutoValue() {
        var map = new HashMap<AutoValueKey, Integer>();
        map.put(AutoValueKey.create("a", 1), 1);

        for (int i = 0; i < 1000000; i++) {
            assertThat(map.get(AutoValueKey.create("a", 1))).isEqualTo(1);
        }
    }

    private static record RecordKey(String a, int b) {}

    @AutoValue
    static abstract class AutoValueKey {
        static AutoValueKey create(String a, int b) {
            return new AutoValue_RecordDemoTest_AutoValueKey(a, b);
        }

        abstract String a();

        abstract int b();
    }
}
