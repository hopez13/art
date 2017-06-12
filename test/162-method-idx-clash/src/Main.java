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

import java.lang.reflect.Method;

public class Main {
    public static void main(String[] args) {
        try {
            Class<?> derived = Class.forName("Derived");
            Object d = derived.newInstance();
            Method dtest = derived.getDeclaredMethod("test");
            dtest.invoke(d);
            Class<?> user = Class.forName("User");
            Method utest = user.getDeclaredMethod("test", derived);
            utest.invoke(null, d);
            Class<?> user2 = Class.forName("User2");
            Method u2test = user2.getDeclaredMethod("test", derived);
            u2test.invoke(null, d);
        } catch (Throwable t) {
            System.out.println("Caught " + t.getClass().getName());
            for (Throwable c = t.getCause(); c != null; c = c.getCause()) {
                System.out.println("  caused by " + c.getClass().getName());
            }
        }
    }
}
