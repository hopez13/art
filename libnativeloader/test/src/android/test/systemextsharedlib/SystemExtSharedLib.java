/*
 * Copyright (C) 2022 The Android Open Source Project
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

package android.test.systemextsharedlib;

// This shared lib gets installed in /system if /system_ext doesn't exist.
// That's only to support the imports in the test apps, but it shouldn't be used
// in that case.
public final class SystemExtSharedLib {
    public static void loadLibrary(String name) {
        System.loadLibrary(name);
    }

    public static void load(String path) {
        System.load(path);
    }
}
