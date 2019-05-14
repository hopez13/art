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

import java.util.ArrayList;
import java.util.List;

/**
 * Class which can parse either dex style signatures (e.g. Lfoo/bar/baz$bat;->foo()V) or javadoc
 * links to class members (e.g. {@link #getClassName()} or {@link java.util.List#clear()}).
 */
public class ApiComponents {
    private String mPackageName;
    private String mClassName;
    private String mMemberName;
    // If the member being referenced is a field, this will always be empty.
    private String mMethodParameterTypes;

    private ApiComponents(String packageName, String className, String memberName,
            String methodParameterTypes) {
        mPackageName = packageName;
        mClassName = className;
        mMemberName = memberName;
        mMethodParameterTypes = methodParameterTypes;
    }

    public String getPackageName() {
        return mPackageName;
    }

    public String getClassName() {
        return mClassName;
    }

    public String getMemberName() {
        return mMemberName;
    }

    public String getMethodParameterTypes() {
        return mMethodParameterTypes;
    }

    private static class PackageAndClassName {
        private final String mPackageName;
        private final String mClassName;

        PackageAndClassName(String packageName, String className) {
            mPackageName = packageName;
            mClassName = className;
        }

        String getPackageName() {
            return mPackageName;
        }

        String getClassName() {
            return mClassName;
        }
    }

    /**
     * Given a potentially fully qualified class name, split it into package and class.
     *
     * @param fullyQualifiedClassName potentially fully qualified class name.
     * @return A pair of strings, containing the package name (or empty if not specified) and the
     *         class name (or empty if string is empty).
     */
    private static PackageAndClassName splitClassName(String fullyQualifiedClassName) {
        int lastDotIdx = fullyQualifiedClassName.lastIndexOf('.');
        if (lastDotIdx == -1) {
            return new PackageAndClassName("", fullyQualifiedClassName);
        }
        String packageName = fullyQualifiedClassName.substring(0, lastDotIdx);
        String className = fullyQualifiedClassName.substring(lastDotIdx + 1);
        return new PackageAndClassName(packageName, className);
    }

    /**
     * Parse a JNI class descriptor. e.g. Lfoo/bar/Baz;
     *
     * @param sc cursor over string assumed to contain a fully qualified JNI class.
     * @return The fully qualified class, in 'dot notation' (e.g. foo.bar.Baz for a class named Baz
     *         in the foo.bar package). The cursor will be placed after the semicolon.
     */
    private static String parseFullyQualifiedJNIClass(StringCursor sc) {
        if (sc.consume() != 'L') {
            sc.rollback();
            throw new IllegalArgumentException(
                    "Expected JNI class name definition to start with L, but instead got " + sc);
        }
        String jniFormattedClassName = sc.consumeUntil(";");

        if (sc.reachedEnd() || sc.consume() != ';') {
            throw new IllegalArgumentException("Class name definition finished unexpectedly.");
        }

        return jniFormattedClassName.replace("/", ".");
    }

    /**
     * Parse a JNI type; can be either a primitive or object type. Arrays are handled separately.
     *
     * @param sc Cursor over the string assumed to contain a JNI type.
     * @return String containing parsed JNI type.
     */
    private static String parseJniTypeWithoutArrayDimensions(StringCursor sc) {
        char c = sc.consume();
        switch (c) {
            case 'Z':
                return "boolean";
            case 'B':
                return "byte";
            case 'C':
                return "char";
            case 'S':
                return "short";
            case 'I':
                return "int";
            case 'J':
                return "long";
            case 'F':
                return "float";
            case 'D':
                return "double";
            case 'L':
                sc.rollback();
                return parseFullyQualifiedJNIClass(sc);
            default:
                throw new RuntimeException("Illegal token " + c + " within signature: " +
                        sc.getOriginalString());
        }
    }

    /**
     * Parse a single dex method parameter.
     *
     * This parameter can be an array, in which case it will be preceded by a number of open square
     * brackets (corresponding to its dimensionality)
     *
     * @param sc Cursor over the string assumed to contain a JNI type.
     * @return Same as {@link #parseJniTypeWithoutArrayDimensions(StringCursor)}, but also handle
     *         arrays.
     */
    private static String parseJniType(StringCursor sc) {
        int arrayDimension = 0;
        while (sc.peek() == '[') {
            ++arrayDimension;
            sc.consume();
        }
        StringBuilder sb = new StringBuilder();
        sb.append(parseJniTypeWithoutArrayDimensions(sc));
        for (int i = 0; i < arrayDimension; ++i) {
            sb.append("[]");
        }
        return sb.toString();
    }

    /**
     * Converts the parameters of method from JNI notation to Javadoc link notation. e.g.
     * "(IILfoo/bar/Baz;)V" turns into "int, int, foo.bar.Baz". The parentheses and return type are
     * discarded.
     *
     * @param sc Cursor over the string assumed to contain a JNI method parameters.
     * @return Comma separated list of parameter types.
     */
    private static String convertJNIMethodParametersToJavadoc(StringCursor sc) {
        List<String> methodParameterTypes = new ArrayList<>();
        if (sc.consume() != '(') {
            throw new IllegalArgumentException("Trying to parse method params of an invalid dex " +
                    "signature: " + sc.getOriginalString());
        }
        while (sc.peek() != ')') {
            methodParameterTypes.add(parseJniType(sc));
        }
        return String.join(", ", methodParameterTypes);
    }

    /**
     * Generate ApiComponents from a dex signature.
     *
     * This is used to extract the necessary context for an alternative API to try to infer missing
     * information.
     *
     * @param signature Dex signature.
     * @return ApiComponents instance with populated package, class name, and parameter types if
     *         applicable.
     */
    public static ApiComponents fromDexSignature(String signature) {
        String packageName = "";
        String className = "";
        String memberName = "";
        String methodParameterTypes = "";
        StringCursor sc = new StringCursor(signature);
        String fullyQualifiedClass = parseFullyQualifiedJNIClass(sc);
        PackageAndClassName packageAndClassName = splitClassName(fullyQualifiedClass);
        packageName = packageAndClassName.getPackageName();
        className = packageAndClassName.getClassName();

        if (!sc.consume(2).equals("->")) {
            throw new IllegalArgumentException("Signature is malformed:" + signature);
        }

        memberName = sc.consumeUntil("(:");
        if (sc.peek() == '(') {
            methodParameterTypes = convertJNIMethodParametersToJavadoc(sc);
        }

        return new ApiComponents(packageName, className, memberName, methodParameterTypes);
    }

    /**
     * Generate ApiComponents from a link tag.
     *
     * @param linkTag          the contents of a link tag.
     * @param contextSignature the signature of the private API that this is an alternative for.
     *                         Used to infer unspecified components.
     */
    public static ApiComponents fromLinkTag(String linkTag, String contextSignature) {
        ApiComponents contextAlternative = fromDexSignature(contextSignature);
        StringCursor sc = new StringCursor(linkTag);
        String packageName = "";
        String className = "";
        String memberName = "";
        String methodParameterTypes = "";

        String fullyQualifiedClassName = sc.consumeUntil("#");
        sc.consume();
        PackageAndClassName packageAndClassName = splitClassName(fullyQualifiedClassName);

        packageName = packageAndClassName.getPackageName();
        className = packageAndClassName.getClassName();

        if (packageName.isEmpty()) {
            packageName = contextAlternative.getPackageName();
        }

        if (className.isEmpty()) {
            className = contextAlternative.getClassName();
        }

        memberName = sc.consumeUntil("(:");

        if (!sc.reachedEnd() && sc.consume() == '(') {
            methodParameterTypes = sc.consumeUntil(")");
        }

        return new ApiComponents(packageName, className, memberName, methodParameterTypes);
    }

    /**
     * Less restrictive comparator to use in case a link tag is missing a method's parameters.
     * e.g. foo.bar.Baz#foo will be considered the same as foo.bar.Baz#foo(int, int) and
     * foo.bar.Baz#foo(long, long). If the class only has one method with that name, then specifying
     * its parameter types is optional within the link tag.
     */
    public boolean equalIgnoringParam(ApiComponents other) {
        return (mPackageName.equals(other.mPackageName)) && (mClassName.equals(other.mClassName)) &&
                (mMemberName.equals(other.mMemberName));
    }
}
