package com.android.class2greylist;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.atLeastOnce;
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

public class CovariantReturnTypeMultiHandlerTest extends AnnotationHandlerTestBase {


    @Before
    public void setup() throws IOException {
        mJavac.addSource("annotation.Whitelist", Joiner.on('\n').join(
                "package annotation;",
                "import static java.lang.annotation.RetentionPolicy.CLASS;",
                "import java.lang.annotation.Repeatable;",
                "import java.lang.annotation.Retention;",
                "@Repeatable(Whitelist.WhitelistMulti.class)",
                "@Retention(CLASS)",
                "public @interface Whitelist {",
                "  Class<?> returnType();",
                "  @Retention(CLASS)",
                "  @interface WhitelistMulti {",
                "    Whitelist[] value();",
                "  }",
                "}"));
    }

    @Test
    public void testReturnTypeMulti() throws IOException {
        mJavac.addSource("a.b.Class", Joiner.on('\n').join(
                "package a.b;",
                "import annotation.Whitelist;",
                "public class Class {",
                "  @Whitelist(returnType=Integer.class)",
                "  @Whitelist(returnType=Long.class)",
                "  public String method() {return null;}",
                "}"));
        assertThat(mJavac.compile()).isTrue();

        Map<String, AnnotationHandler> handlerMap =
                ImmutableMap.of("Lannotation/Whitelist$WhitelistMulti;",
                        new CovariantReturnTypeMultiHandler(
                                mConsumer,
                                ImmutableSet.of("La/b/Class;->method()Ljava/lang/String;"),
                                "Lannotation/Whitelist;"));
        new AnnotationVisitor(mJavac.getCompiledClass("a.b.Class"), mStatus, handlerMap).visit();

        assertNoErrors();
        ArgumentCaptor<String> whitelist = ArgumentCaptor.forClass(String.class);
        verify(mConsumer, times(2)).whitelistEntry(whitelist.capture());
        assertThat(whitelist.getAllValues()).containsExactly(
                "La/b/Class;->method()Ljava/lang/Integer;",
                "La/b/Class;->method()Ljava/lang/Long;"
        );
    }

    @Test
    public void testReturnTypeMultiNotPublicApi() throws IOException {
        mJavac.addSource("a.b.Class", Joiner.on('\n').join(
                "package a.b;",
                "import annotation.Whitelist;",
                "public class Class {",
                "  @Whitelist(returnType=Integer.class)",
                "  @Whitelist(returnType=Long.class)",
                "  public String method() {return null;}",
                "}"));
        assertThat(mJavac.compile()).isTrue();

        Map<String, AnnotationHandler> handlerMap =
                ImmutableMap.of("Lannotation/Whitelist$WhitelistMulti;",
                        new CovariantReturnTypeMultiHandler(
                                mConsumer,
                                emptySet(),
                                "Lannotation/Whitelist;"));
        new AnnotationVisitor(mJavac.getCompiledClass("a.b.Class"), mStatus, handlerMap).visit();

        verify(mStatus, atLeastOnce()).error(any(), any());
    }
}
