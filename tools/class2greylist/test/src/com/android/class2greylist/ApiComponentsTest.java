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

import static com.google.common.truth.Truth.assertThat;

import org.junit.Test;

public class ApiComponentsTest extends AnnotationHandlerTestBase {
    private static final String METHOD_SIGNATURE_WITHOUT_PARAMS = "La/b/C;->foo()V";
    private static final String METHOD_SIGNATURE_WITH_PARAMS = "La/b/C;->foo(IJLfoo2/bar/Baz;)V";
    private static final String FIELD_SIGNATURE = "La/b/C;->foo:I";

    private static final String LINK_NO_PACKAGE_NO_CLASS = "#foo(int, long, foo2.bar.Baz)";
    private static final String LINK_NO_PACKAGE = "C#foo(int, long, foo2.bar.Baz)";
    private static final String LINK = "a.b.C#foo(int, long, foo2.bar.Baz)";
    private static final String LINK_NO_PARAMS = "a.b.C#foo";

    @Test
    public void testGetApiComponentsPackageFromSignature() {
        ApiComponents api = ApiComponents.FromSignature(METHOD_SIGNATURE_WITHOUT_PARAMS);
        assertThat(api.getPackageName()).isEqualTo("a.b");
    }

    @Test
    public void testGetApiComponentsClassNameFromSignature() {
        ApiComponents api = ApiComponents.FromSignature(METHOD_SIGNATURE_WITHOUT_PARAMS);
        assertThat(api.getClassName()).isEqualTo("C");
    }

    @Test
    public void testGetApiComponentsFieldNameFromSignature() {
        ApiComponents api = ApiComponents.FromSignature(METHOD_SIGNATURE_WITHOUT_PARAMS);
        assertThat(api.getFieldName()).isEqualTo("foo");
    }

    @Test
    public void testGetApiComponentsNoParametersFromSignature() {
        ApiComponents api = ApiComponents.FromSignature(METHOD_SIGNATURE_WITHOUT_PARAMS);
        assertThat(api.getParameters()).isEqualTo("");
    }

    @Test
    public void testGetApiComponentsParametersFromSignature() {
        ApiComponents api = ApiComponents.FromSignature(METHOD_SIGNATURE_WITH_PARAMS);
        assertThat(api.getParameters()).isEqualTo("int, long, foo2.bar.Baz");
    }

    @Test
    public void testGetApiComponentsPackageFromLink() {
        ApiComponents api = ApiComponents.FromLinkTag(LINK, FIELD_SIGNATURE);
        assertThat(api.getPackageName()).isEqualTo("a.b");
    }

    @Test
    public void testGetApiComponentsClassNameFromLink() {
        ApiComponents api = ApiComponents.FromLinkTag(LINK, FIELD_SIGNATURE);
        assertThat(api.getClassName()).isEqualTo("C");
    }

    @Test
    public void testGetApiComponentsFieldNameFromLink() {
        ApiComponents api = ApiComponents.FromLinkTag(LINK, FIELD_SIGNATURE);
        assertThat(api.getFieldName()).isEqualTo("foo");
    }

    @Test
    public void testGetApiComponentsNoParametersFromLink() {
        ApiComponents api = ApiComponents.FromLinkTag(LINK_NO_PARAMS, FIELD_SIGNATURE);
        assertThat(api.getParameters()).isEqualTo("");
    }

    @Test
    public void testGetApiComponentsParametersFromLink() {
        ApiComponents api = ApiComponents.FromLinkTag(LINK, FIELD_SIGNATURE);
        assertThat(api.getParameters()).isEqualTo("int, long, foo2.bar.Baz");
    }

    @Test
    public void testDeduceApiComponentsPackageFromLinkUsingContext() {
        ApiComponents api = ApiComponents.FromLinkTag(LINK_NO_PACKAGE, FIELD_SIGNATURE);
        assertThat(api.getPackageName()).isEqualTo("a.b");
    }

    @Test
    public void testDeduceApiComponentsPackageAndClassFromLinkUsingContext() {
        ApiComponents api = ApiComponents.FromLinkTag(LINK_NO_PACKAGE_NO_CLASS, FIELD_SIGNATURE);
        assertThat(api.getPackageName()).isEqualTo("a.b");
        assertThat(api.getClassName()).isEqualTo("C");
    }


}