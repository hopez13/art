/*
 * Copyright (C) 2019 The Android Open Source Project
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

package com.android.class2greylist;

import static org.junit.Assert.fail;
import static com.google.common.truth.Truth.assertThat;

import org.junit.Test;

public class ApiComponentsTest extends AnnotationHandlerTestBase {

    private final ErrorReporter mErrorReporter = (error, objects) -> fail(
            "Unexpected failure: " + error);

    @Test
    public void testGetApiComponentsPackageFromSignature() {
        ApiComponents api = ApiComponents.fromDexSignature("La/b/C;->foo()V");
        PackageAndClassName packageAndClassName = api.getPackageAndClassName();
        assertThat(packageAndClassName.packageName).isEqualTo("a.b");
    }

    @Test
    public void testGetApiComponentsFromSignature() {
        ApiComponents api = ApiComponents.fromDexSignature("La/b/C;->foo(IJLfoo2/bar/Baz;)V");
        PackageAndClassName packageAndClassName = api.getPackageAndClassName();
        assertThat(packageAndClassName.className).isEqualTo("C");
        assertThat(api.getMemberName()).isEqualTo("foo");
        assertThat(api.getMethodParameterTypes()).isEqualTo("int, long, foo2.bar.Baz");
    }

    @Test
    public void testGetApiComponentsFromFieldLink() {
        ApiComponents api = ApiComponents.fromLinkTag("a.b.C#foo(int, long, foo2.bar.Baz)",
                "La/b/C;->foo:I", mErrorReporter);
        PackageAndClassName packageAndClassName = api.getPackageAndClassName();
        assertThat(packageAndClassName.packageName).isEqualTo("a.b");
        assertThat(packageAndClassName.className).isEqualTo("C");
        assertThat(api.getMemberName()).isEqualTo("foo");
    }

    @Test
    public void testGetApiComponentsLinkOnlyClass() {
        ApiComponents api = ApiComponents.fromLinkTag("b.c.D",
                "La/b/C;->foo:I", mErrorReporter);
        PackageAndClassName packageAndClassName = api.getPackageAndClassName();
        assertThat(packageAndClassName.packageName).isEqualTo("b.c");
        assertThat(packageAndClassName.className).isEqualTo("D");
        assertThat(api.getMethodParameterTypes()).isEqualTo("");
    }

    @Test
    public void testGetApiComponentsFromLinkOnlyClassDeducePackage() {
        ApiComponents api = ApiComponents.fromLinkTag("D",
                "La/b/C;->foo:I", mErrorReporter);
        PackageAndClassName packageAndClassName = api.getPackageAndClassName();
        assertThat(packageAndClassName.packageName).isEqualTo("a.b");
        assertThat(packageAndClassName.className).isEqualTo("D");
        assertThat(api.getMethodParameterTypes()).isEqualTo("");
    }

    @Test
    public void testGetApiComponentsParametersFromMethodLink() {
        ApiComponents api = ApiComponents.fromLinkTag("a.b.C#foo(int, long, foo2.bar.Baz)",
                "La/b/C;->foo:I", mErrorReporter);
        assertThat(api.getMethodParameterTypes()).isEqualTo("int, long, foo2.bar.Baz");
    }

    @Test
    public void testDeduceApiComponentsPackageFromLinkUsingContext() {
        ApiComponents api = ApiComponents.fromLinkTag("C#foo(int, long, foo2.bar.Baz)",
                "La/b/C;->foo:I",
                mErrorReporter);
        PackageAndClassName packageAndClassName = api.getPackageAndClassName();
        assertThat(packageAndClassName.packageName).isEqualTo("a.b");
    }

    @Test
    public void testDeduceApiComponentsPackageAndClassFromLinkUsingContext() {
        ApiComponents api = ApiComponents.fromLinkTag("#foo(int, long, foo2.bar.Baz)",
                "La/b/C;->foo:I",
                mErrorReporter);
        PackageAndClassName packageAndClassName = api.getPackageAndClassName();
        assertThat(packageAndClassName.packageName).isEqualTo("a.b");
        assertThat(packageAndClassName.className).isEqualTo("C");
    }
}