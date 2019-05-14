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

public class ApiComponents {
    private String mPackageName;
    private String mClassName;
    private String mFieldName;
    private String mParameters;

    private ApiComponents(String packageName, String className, String fieldName,
            String parameters) {
        mPackageName = packageName;
        mClassName = className;
        mFieldName = fieldName;
        mParameters = parameters;
    }

    public String getPackageName() {
        return mPackageName;
    }

    public String getClassName() {
        return mClassName;
    }

    public String getFieldName() {
        return mFieldName;
    }

    public String getParameters() {
        return mParameters;
    }

    private static String JNIToJavadocType(char c) {
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
            default:
                throw new RuntimeException("Illegal token within signature:" + c);
        }
    }

    private static String ConvertSignatureParamsToJavadoc(String signature) {
        List<String> parameterTypes = new ArrayList<>();
        boolean processingClass = false;
        int classStartIndex = -1;
        int arrayDimensionality = 0;
        for (int index = 0; index < signature.length(); ++index) {
            char c = signature.charAt(index);
            if (c == '[') {
                ++arrayDimensionality;
            } else if (processingClass) {
                if (c == ';') {
                    StringBuilder toAdd = new StringBuilder();
                    toAdd.append(signature.substring(classStartIndex + 1, index)
                            .replace('/', '.'));
                    for (int i = 0; i < arrayDimensionality; ++i) {
                        toAdd.append("[]");
                    }
                    parameterTypes.add(toAdd.toString());
                    processingClass = false;
                    arrayDimensionality = 0;
                }
            } else {
                if (c == ')') {
                    break;
                } else if (c == 'L') {
                    processingClass = true;
                    classStartIndex = index;
                } else {
                    StringBuilder toAdd = new StringBuilder();
                    toAdd.append(JNIToJavadocType(c));
                    for (int i = 0; i < arrayDimensionality; ++i) {
                        toAdd.append("[]");
                    }
                    parameterTypes.add(toAdd.toString());
                    arrayDimensionality = 0;
                }
            }
        }
        return String.join(", ", parameterTypes);
    }

    /**
     * Generate ApiComponents from a JNI signature.
     *
     * This is used to extract the necessary context for an alternative API, to try to infer missing
     * information.
     *
     * @param signature JNI signature.
     */
    public static ApiComponents FromSignature(String signature) {
        String[] parts = signature.split("->");
        if (parts.length != 2) {
            throw new RuntimeException("Illegal signature " + signature);
        }
        // Strip L and ;
        String packageAndClass = parts[0].substring(1, parts[0].length() - 1);
        String fieldAndParam = parts[1];

        int lastSlashIndex = packageAndClass.lastIndexOf('/');
        if (lastSlashIndex <= 0) {
            throw new RuntimeException("Signature lacks a package and/or class:" +
                    signature);
        }
        String packageName = packageAndClass.substring(0, lastSlashIndex).replace("/", ".");
        String className = packageAndClass.substring(lastSlashIndex + 1);
        String fieldName = "";
        String parameters = "";
        int indexOfLeftParen = fieldAndParam.indexOf('(');
        if (indexOfLeftParen > 0) {
            // It's a method field
            fieldName = fieldAndParam.substring(0, indexOfLeftParen);
            parameters =
                    ConvertSignatureParamsToJavadoc(fieldAndParam.substring(indexOfLeftParen + 1));
        } else {
            // It's a variable field
            fieldName = fieldAndParam.split(":")[0];
        }
        return new ApiComponents(packageName, className, fieldName, parameters);
    }

    /**
     * Generate ApiComponents from a link tag.
     *
     * @param linkTag          the contents of a link tag.
     * @param contextSignature the signature of the private API that this is an alternative for.
     *                         Used to infer unspecified parameters.
     */
    public static ApiComponents FromLinkTag(String linkTag, String contextSignature) {
        ApiComponents contextAlternative = FromSignature(contextSignature);
        String packageName = "";
        String className = "";
        String fieldName = "";
        String parameters = "";
        String[] parts = linkTag.split("#");
        String fieldAndParams = "";
        if (parts.length > 2) {
            throw new RuntimeException("Malformed link tag:" + linkTag);
        } else if (parts.length == 1) {
            packageName = contextAlternative.mPackageName;
            className = contextAlternative.mClassName;
            fieldAndParams = parts[0];
        } else {
            if (parts[0].length() == 0) {
                packageName = contextAlternative.mPackageName;
                className = contextAlternative.mClassName;
            } else {
                int lastDotIndex = parts[0].lastIndexOf('.');
                if (lastDotIndex == -1) {
                    packageName = contextAlternative.mPackageName;
                    className = parts[0];
                } else {
                    packageName = parts[0].substring(0, lastDotIndex);
                    className = parts[0].substring(lastDotIndex + 1);
                }
            }

            fieldAndParams = parts[1];
        }

        int leftParenIndex = fieldAndParams.indexOf('(');
        if (leftParenIndex == -1) {
            fieldName = fieldAndParams;
        } else {
            fieldName = fieldAndParams.substring(0, leftParenIndex);
            parameters = fieldAndParams.substring(leftParenIndex + 1,
                    fieldAndParams.length() - 1);
        }

        return new ApiComponents(packageName, className, fieldName, parameters);
    }

    public boolean almostEqual(ApiComponents other) {
        return (mPackageName.equals(other.mPackageName)) && (mClassName.equals(other.mClassName)) &&
                (mFieldName.equals(other.mFieldName));
    }
}
