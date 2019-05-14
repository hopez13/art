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

import static org.junit.Assert.fail;

import org.junit.Test;

import java.util.Arrays;
import java.util.Collections;
import java.util.Set;
import java.util.TreeSet;

public class ApiResolverTest extends AnnotationHandlerTestBase {

    @Test
    public void testFindPublicAlternativeExactly() {
        Set<String> publicApis = Collections.unmodifiableSet(new TreeSet<String>(
                Arrays.asList("La/b/C;->foo(I)V", "La/b/C;->bar(I)V")));
        ApiResolver resolver = new ApiResolver(publicApis);
        assertThat(resolver.resolvePublicAlternatives("{@link a.b.C#foo(int)}", "Lb/c/D;->bar()V",
                (message, objects) -> fail("Error during parsing: " + message))).isTrue();
    }

    @Test
    public void testFindPublicAlternativeDeducedPackageName() {
        Set<String> publicApis = Collections.unmodifiableSet(new TreeSet<String>(
                Arrays.asList("La/b/C;->foo(I)V", "La/b/C;->bar(I)V")));
        ApiResolver resolver = new ApiResolver(publicApis);
        assertThat(resolver.resolvePublicAlternatives("{@link C#foo(int)}", "La/b/D;->bar()V",
                new NoopErrorReporter())).isTrue();
    }

    @Test
    public void testFindPublicAlternativeDeducedPackageAndClassName() {
        Set<String> publicApis = Collections.unmodifiableSet(new TreeSet<String>(
                Arrays.asList("La/b/C;->foo(I)V", "La/b/C;->bar(I)V")));
        ApiResolver resolver = new ApiResolver(publicApis);
        assertThat(resolver.resolvePublicAlternatives("{@link #foo(int)}", "La/b/C;->bar()V",
                new NoopErrorReporter())).isTrue();
    }

    @Test
    public void testFindPublicAlternativeDeducedParameterTypes() {
        Set<String> publicApis = Collections.unmodifiableSet(new TreeSet<String>(
                Arrays.asList("La/b/C;->foo(I)V", "La/b/C;->bar(I)V")));
        ApiResolver resolver = new ApiResolver(publicApis);
        assertThat(resolver.resolvePublicAlternatives("{@link #foo}", "La/b/C;->bar()V",
                new NoopErrorReporter())).isTrue();
    }

    @Test
    public void testFindPublicAlternativeFailDueToMultipleParameterTypes() {
        Set<String> publicApis = Collections.unmodifiableSet(new TreeSet<String>(
                Arrays.asList("La/b/C;->foo(I)V", "La/b/C;->foo(II)V")));
        ApiResolver resolver = new ApiResolver(publicApis);
        StringErrorReporter reporter = new StringErrorReporter();
        assertThat(resolver.resolvePublicAlternatives("{@link #foo}", "La/b/C;->bar()V",
                reporter)).isFalse();
        assertThat(reporter.getError()).isEqualTo("Alternative #foo returned multiple "
                + "matches: a.b.C#foo(int), a.b.C#foo(int, int)");
    }

    @Test
    public void testFindPublicAlternativeFailNoAlternative() {
        Set<String> publicApis = Collections.unmodifiableSet(new TreeSet<String>(
                Arrays.asList("La/b/C;->bar(I)V")));
        ApiResolver resolver = new ApiResolver(publicApis);
        StringErrorReporter reporter = new StringErrorReporter();
        assertThat(resolver.resolvePublicAlternatives("{@link #foo(int)}", "La/b/C;->bar()V",
                reporter)).isFalse();
        assertThat(reporter.getError()).isEqualTo("Could not find public api #foo(int).");
    }

    @Test
    public void testFindPublicAlternativeFailNoAlternativeNoParameterTypes() {
        Set<String> publicApis = Collections.unmodifiableSet(new TreeSet<String>(
                Arrays.asList("La/b/C;->bar(I)V")));
        ApiResolver resolver = new ApiResolver(publicApis);
        StringErrorReporter reporter = new StringErrorReporter();
        assertThat(resolver.resolvePublicAlternatives("{@link #foo}", "La/b/C;->bar()V",
                reporter)).isFalse();
        assertThat(reporter.getError()).isEqualTo(
                "Alternative #foo returned no public API matches.");
    }

    @Test
    public void testNoPublicAlternatives() {
        Set<String> publicApis = Collections.unmodifiableSet(new TreeSet<String>());
        ApiResolver resolver = new ApiResolver(publicApis);

        assertThat(resolver.resolvePublicAlternatives("Foo", "La/b/C;->bar()V",
                new NoopErrorReporter())).isFalse();
    }

    @Test
    public void testNoPublicAlternativesButHasExplanation() {
        Set<String> publicApis = Collections.unmodifiableSet(new TreeSet<String>());
        ApiResolver resolver = new ApiResolver(publicApis);

        assertThat(resolver.resolvePublicAlternatives("Foo {@code bar}", "La/b/C;->bar()V",
                new NoopErrorReporter())).isTrue();
    }

}