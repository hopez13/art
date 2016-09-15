## -*- coding: utf-8 -*-
##
## Copyright (C) 2016 The Android Open Source Project
##
##  Licensed under the Apache License, Version 2.0 (the "License");
##  you may not use this file except in compliance with the License.
##  You may obtain a copy of the License at
##
##       http://www.apache.org/licenses/LICENSE-2.0
##
##  Unless required by applicable law or agreed to in writing, software
##  distributed under the License is distributed on an "AS IS" BASIS,
##  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
##  See the License for the specific language governing permissions and
##  limitations under the License.
##
/*
 * Copyright (C) 2007 The Android Open Source Project
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

/*
 * AUTOMATICALLY GENERATED FROM art/tools/mako-source-generator/...../org_apache_harmony_dalvik_NativeTestTarget.cpp.mako
 *
 * !!! DO NOT EDIT DIRECTLY !!!
 *
 */

#define LOG_TAG "NativeTestTarget"

#include "JNIHelp.h"
#include "JniConstants.h"

static void NativeTestTarget_emptyJniStaticSynchronizedMethod0(JNIEnv*, jclass) { }
static void NativeTestTarget_emptyJniSynchronizedMethod0(JNIEnv*, jclass) { }

static JNINativeMethod gMethods_NormalOnly[] = {
    NATIVE_METHOD(NativeTestTarget, emptyJniStaticSynchronizedMethod0, "()V"),
    NATIVE_METHOD(NativeTestTarget, emptyJniSynchronizedMethod0, "()V"),
};

% for explicit_register, jni_suffix in [(False, ""), (True, "_Fast"), (True, "_Critical")]:
<%
test_refs = jni_suffix != "_Critical"
jni_extra_args_only = test_refs and "JNIEnv*, jclass" or ""
jni_extra_args = test_refs and "JNIEnv*, jclass," or ""
%>
% if test_refs:
static void NativeTestTarget_emptyJniMethod0${jni_suffix}(JNIEnv*, jobject) { }
static void NativeTestTarget_emptyJniMethod6${jni_suffix}(JNIEnv*, jobject, int, int, int, int, int, int) { }
static void NativeTestTarget_emptyJniMethod6L${jni_suffix}(JNIEnv*, jobject, jobject, jarray, jarray, jobject, jarray, jarray) { }
static void NativeTestTarget_emptyJniStaticMethod6L${jni_suffix}(JNIEnv*, jclass, jobject, jarray, jarray, jobject, jarray, jarray) { }
% endif

static void NativeTestTarget_emptyJniStaticMethod0${jni_suffix}(${jni_extra_args_only}) { }
static void NativeTestTarget_emptyJniStaticMethod6${jni_suffix}(${jni_extra_args} int, int, int, int, int, int) { }

static JNINativeMethod gMethods${jni_suffix}[] = {
% if test_refs:
    NATIVE_METHOD(NativeTestTarget, emptyJniMethod0${jni_suffix}, "()V"),
    NATIVE_METHOD(NativeTestTarget, emptyJniMethod6${jni_suffix}, "(IIIIII)V"),
    NATIVE_METHOD(NativeTestTarget, emptyJniMethod6L${jni_suffix}, "(Ljava/lang/String;[Ljava/lang/String;[[ILjava/lang/Object;[Ljava/lang/Object;[[[[Ljava/lang/Object;)V"),
    NATIVE_METHOD(NativeTestTarget, emptyJniStaticMethod6L${jni_suffix}, "(Ljava/lang/String;[Ljava/lang/String;[[ILjava/lang/Object;[Ljava/lang/Object;[[[[Ljava/lang/Object;)V"),
% endif
    NATIVE_METHOD(NativeTestTarget, emptyJniStaticMethod0${jni_suffix}, "()V"),
    NATIVE_METHOD(NativeTestTarget, emptyJniStaticMethod6${jni_suffix}, "(IIIIII)V"),
};
% endfor
int register_org_apache_harmony_dalvik_NativeTestTarget(JNIEnv* env) {
    jniRegisterNativeMethods(env, "org/apache/harmony/dalvik/NativeTestTarget", gMethods_NormalOnly, NELEM(gMethods_NormalOnly));
    jniRegisterNativeMethods(env, "org/apache/harmony/dalvik/NativeTestTarget", gMethods, NELEM(gMethods));
    jniRegisterNativeMethods(env, "org/apache/harmony/dalvik/NativeTestTarget", gMethods_Fast, NELEM(gMethods_Fast));
    jniRegisterNativeMethods(env, "org/apache/harmony/dalvik/NativeTestTarget", gMethods_Critical, NELEM(gMethods_Critical));

    return 0;
}
