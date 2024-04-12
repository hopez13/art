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
 * limitations under the License.
 */

public class Main {
    public static void main(String args[]) {
        System.out.println($noinline$getAppImageClass().getName());
        System.out.println($noinline$getNonAppImageClass().getName());
    }

    /// CHECK-START: java.lang.Class Main.$noinline$getAppImageClass() builder (after)
    /// CHECK:            LoadClass load_kind:AppImageRelRo in_image:true
    public static Class<?> $noinline$getAppImageClass() {
        return AppImageClass.class;
    }

    /// CHECK-START: java.lang.Class Main.$noinline$getNonAppImageClass() builder (after)
    /// CHECK:            LoadClass load_kind:BssEntry in_image:false
    public static Class<?> $noinline$getNonAppImageClass() {
        return NonAppImageClass.class;
    }
}

class AppImageClass {}  // Included in profile.
class NonAppImageClass {}  // Not included in the profile.
