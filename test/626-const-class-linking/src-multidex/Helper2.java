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

public class Helper2 {
    public static Class<?> get() {
        System.out.println("Helper2 class loader: " + Helper2.class.getClassLoader().getClass());
        Class<?> k = Test.class;
        System.out.println("Test class loader: " + k.getClassLoader().getClass());
        return k;
    }
}
