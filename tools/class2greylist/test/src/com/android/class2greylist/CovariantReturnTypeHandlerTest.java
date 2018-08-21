/*
 * Copyright (C) 2018 The Android Open Source Project
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

package com.android.class2greylist;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import static java.util.Collections.emptySet;

import com.google.common.base.Joiner;
import com.google.common.collect.ImmutableMap;
import com.google.common.collect.ImmutableSet;

import org.junit.Before;
import org.junit.Test;
import org.mockito.ArgumentCaptor;

import java.io.IOException;
import java.util.Map;

public class CovariantReturnTypeHandlerTest extends AnnotationHandlerTestBase {

    private static final String ANNOTATION = "Lannotation/Annotation;";

    @Before
    public void setup() throws IOException {
        mJavac.addSource("annotation.Annotation", Joiner.on('\n').join(
                "package annotation;",
                "import static java.lang.annotation.RetentionPolicy.CLASS;",
                "import java.lang.annotation.Retention;",
                "@Retention(CLASS)",
                "public @interface Annotation {",
                "  Class<?> returnType();",
                "}"));
    }

    @Test
    public void testReturnType() throws IOException {
        mJavac.addSource("a.b.Class", Joiner.on('\n').join(
                "package a.b;",
                "import annotation.Annotation;",
                "public class Class {",
                "  @Annotation(returnType=Integer.class)",
                "  public String method() {return null;}",
                "}"));
        assertThat(mJavac.compile()).isTrue();

        Map<String, AnnotationHandler> handlerMap =
                ImmutableMap.of(ANNOTATION,
                        new CovariantReturnTypeHandler(
                                mConsumer,
                                ImmutableSet.of("La/b/Class;->method()Ljava/lang/String;")));
        new AnnotationVisitor(mJavac.getCompiledClass("a.b.Class"), mStatus, handlerMap).visit();

        assertNoErrors();
        ArgumentCaptor<String> whitelist = ArgumentCaptor.forClass(String.class);
        verify(mConsumer, times(1)).whitelistEntry(whitelist.capture());
        assertThat(whitelist.getValue()).isEqualTo("La/b/Class;->method()Ljava/lang/Integer;");
    }

    @Test
    public void testReturnTypeNotPublicApi() throws IOException {
        mJavac.addSource("a.b.Class", Joiner.on('\n').join(
                "package a.b;",
                "import annotation.Annotation;",
                "public class Class {",
                "  @Annotation(returnType=Integer.class)",
                "  public String method() {return null;}",
                "}"));
        assertThat(mJavac.compile()).isTrue();

        Map<String, AnnotationHandler> handlerMap =
                ImmutableMap.of(ANNOTATION,
                        new CovariantReturnTypeHandler(
                                mConsumer,
                                emptySet()));
        new AnnotationVisitor(mJavac.getCompiledClass("a.b.Class"), mStatus, handlerMap).visit();

        verify(mStatus, times(1)).error(any(), any());
    }
}
