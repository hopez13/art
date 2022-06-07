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
        //testSmaliAnnotatedClasses();
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

    private static void testSmaliAnnotatedClasses() {
        try {
            Class hostClass = Class.forName("libcore.java.lang.nestgroup.NestGroupHost");
            Class innerAClass = Class.forName("libcore.java.lang.nestgroup.NestGroupInnerA");
            Class innerBClass = Class.forName("libcore.java.lang.nestgroup.NestGroupInnerB");

            Object host = hostClass.newInstance();
            Object innerA = innerAClass.getConstructor(new Class[] { hostClass }).newInstance(host);
            Object innerB = innerBClass.getConstructor(new Class[] { hostClass }).newInstance(host);

            Method testInnerAccess = hostClass.getMethod("testInnerAccess", (Class[]) null);
            Method reset = hostClass.getMethod("reset", (Class[]) null);
            check("smali.testInnerAccess", 4, (int)testInnerAccess.invoke(host));
            check("smali.testInnerAccess", 8, (int)testInnerAccess.invoke(host));
            check("smali.testInnerAccess", 12, (int)testInnerAccess.invoke(host));

            reset.invoke(host);
            Method testHostAccessA = innerAClass.getMethod("testHostAccess", (Class[]) null);
            Method testMemberAccessA = innerAClass.getMethod("testMemberAccess", (Class[]) null);
            check("smali.testHostAccess", 2, (int)testHostAccessA.invoke(innerA));
            check("smali.testHostAccess", 4, (int)testHostAccessA.invoke(innerA));
            check("smali.testHostAccess", 6, (int)testHostAccessA.invoke(innerA));
            check("smali.testMemberAccess", 2, (int)testMemberAccessA.invoke(innerA));
            check("smali.testMemberAccess", 4, (int)testMemberAccessA.invoke(innerA));
            check("smali.testMemberAccess", 6, (int)testMemberAccessA.invoke(innerA));

            reset.invoke(host);
            Method testHostAccessB = innerBClass.getMethod("testHostAccess", (Class[]) null);
            Method testMemberAccessB = innerBClass.getMethod("testMemberAccess", (Class[]) null);
            check("smali.testHostAccess", 2, (int)testHostAccessB.invoke(innerB));
            check("smali.testHostAccess", 4, (int)testHostAccessB.invoke(innerB));
            check("smali.testHostAccess", 6, (int)testHostAccessB.invoke(innerB));
            check("smali.testMemberAccess", 2, (int)testMemberAccessB.invoke(innerB));
            check("smali.testMemberAccess", 4, (int)testMemberAccessB.invoke(innerB));
            check("smali.testMemberAccess", 6, (int)testMemberAccessB.invoke(innerB));
        } catch (Throwable t) {
            throw new RuntimeException(t);
        }
    }

    private static void check(String msg, int actual, int expected) {
        if (actual != expected) {
            System.out.println(msg + " : " + actual + " != " + expected);
            System.exit(1);
        }
    }
}
