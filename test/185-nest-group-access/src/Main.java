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

import java.lang.reflect.Method;
import testpackage.TestNestedClasses;

public class Main {
    public static void main(String[] args) {
        testNestedClass();
    }

    private static void testNestedClass() {
        TestNestedClasses host = new TestNestedClasses();
        check("nested.testInnerAccess", 4, host.testInnerAccess());
        check("nested.testInnerAccess", 8, host.testInnerAccess());
        check("nested.testInnerAccess", 12, host.testInnerAccess());

        host.reset();
        check("nested.testHostAccess", 2, host.innerA.testHostAccess());
        check("nested.testHostAccess", 4, host.innerA.testHostAccess());
        check("nested.testHostAccess", 6, host.innerA.testHostAccess());
        check("nested.testMemberAccess", 2, host.innerA.testMemberAccess());
        check("nested.testMemberAccess", 4, host.innerA.testMemberAccess());
        check("nested.testMemberAccess", 6, host.innerA.testMemberAccess());

        host.reset();
        check("nested.testHostAccess", 2, host.innerB.testHostAccess());
        check("nested.testHostAccess", 4, host.innerB.testHostAccess());
        check("nested.testHostAccess", 6, host.innerB.testHostAccess());
        check("nested.testMemberAccess", 2, host.innerB.testMemberAccess());
        check("nested.testMemberAccess", 4, host.innerB.testMemberAccess());
        check("nested.testMemberAccess", 6, host.innerB.testMemberAccess());
    }

    private static void check(String msg, int actual, int expected) {
        if (actual != expected) {
            System.out.println(msg + " : " + actual + " != " + expected);
            System.exit(1);
        }
    }
}
