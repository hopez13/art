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

public class Test5User {
    public static void test() {
        Test5Base b = new Test5Derived();

        // Call an unresolved method bar() to force verification at runtime
        // to populate the dex cache entry for Test5Base.foo()V.
        try { b.bar(); } catch (IncompatibleClassChangeError icce) { }

        b.foo();
    }
}
