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

// Note: This is src3/BaseWithStaticFoo.java and src/BaseWithStaticFoo.java (identical),
// src3/BaseWithStaticFoo.java shall be used in the test.
public class BaseWithStaticFoo {
    public static void foo() {
        System.out.println("BaseWithStaticFoo.foo()");
    }
}
