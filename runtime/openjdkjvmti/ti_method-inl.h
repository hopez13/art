/* Copyright (C) 2016 The Android Open Source Project
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This file implements interfaces from the file jvmti.h. This implementation
 * is licensed under the same terms as the file jvmti.h.  The
 * copyright and license information for the file jvmti.h follows.
 *
 * Copyright (c) 2003, 2011, Oracle and/or its affiliates. All rights reserved.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.  Oracle designates this
 * particular file as subject to the "Classpath" exception as provided
 * by Oracle in the LICENSE file that accompanied this code.
 *
 * This code is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * version 2 for more details (a copy is included in the LICENSE file that
 * accompanied this code).
 *
 * You should have received a copy of the GNU General Public License version
 * 2 along with this work; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Please contact Oracle, 500 Oracle Parkway, Redwood Shores, CA 94065 USA
 * or visit www.oracle.com if you need additional information or have any
 * questions.
 */

#ifndef ART_RUNTIME_OPENJDKJVMTI_TI_METHOD_INL_H_
#define ART_RUNTIME_OPENJDKJVMTI_TI_METHOD_INL_H_

#include "ti_method.h"

namespace openjdkjvmti {
namespace impl {

template<typename T> void WriteJvalue(T, jvalue*);
template<typename T> void ReadJvalue(jvalue, T*);
template<typename T> char GetJNITypeChar();

#define FORALL_JVALUE_TYPES(fn) \
    fn(jboolean, 'Z', z) \
    fn(jbyte, 'B', b) \
    fn(jchar, 'C', c) \
    fn(jshort, 'S', s) \
    fn(jint, 'I', i) \
    fn(jlong, 'J', j) \
    fn(jfloat, 'F', f) \
    fn(jdouble, 'D', d) \
    fn(jobject, 'L', l)

#define JNI_TYPE_CHAR(type, chr, id) \
template<> char GetJNITypeChar<type>() { \
  return chr; \
}

FORALL_JVALUE_TYPES(JNI_TYPE_CHAR);

#undef JNI_TYPE_CHAR

#define RW_JVALUE(type, chr, id) \
    template<> void ReadJvalue<type>(jvalue in, type* out) { \
      *out = in.id; \
    } \
    template<> void WriteJvalue<type>(type in, jvalue* out) { \
      out->id = in; \
    }

FORALL_JVALUE_TYPES(RW_JVALUE);

#undef RW_JVALUE

#undef FORALL_JVALUE_TYPES
}  // namespace impl

template<typename T>
jvmtiError MethodUtil::SetLocalVariable(jvmtiEnv* env,
                                        jthread thread,
                                        jint depth,
                                        jint slot,
                                        T data) {
  jvalue v = {.j = 0};
  char type = impl::GetJNITypeChar<T>();
  impl::WriteJvalue(data, &v);
  return SetLocalVariableGeneric(env, thread, depth, slot, type, v);
}

template<typename T>
jvmtiError MethodUtil::GetLocalVariable(jvmtiEnv* env,
                                        jthread thread,
                                        jint depth,
                                        jint slot,
                                        T* data) {
  if (data == nullptr) {
    return ERR(NULL_POINTER);
  }
  jvalue v = {.j = 0};
  char type = impl::GetJNITypeChar<T>();
  jvmtiError err = GetLocalVariableGeneric(env, thread, depth, slot, type, &v);
  if (err != OK) {
    return err;
  } else {
    impl::ReadJvalue(v, data);
    return OK;
  }
}

}  // namespace openjdkjvmti

#endif  // ART_RUNTIME_OPENJDKJVMTI_TI_METHOD_INL_H_
