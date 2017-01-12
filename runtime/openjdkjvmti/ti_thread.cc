/* Copyright (C) 2017 The Android Open Source Project
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

#include "ti_thread.h"

#include "art_field.h"
#include "art_jvmti.h"
#include "base/logging.h"
#include "base/mutex.h"
#include "jni_internal.h"
#include "mirror/class.h"
#include "mirror/object-inl.h"
#include "obj_ptr.h"
#include "scoped_thread_state_change-inl.h"
#include "thread-inl.h"
#include "well_known_classes.h"

namespace openjdkjvmti {

jvmtiError ThreadUtil::GetCurrentThread(jvmtiEnv* env ATTRIBUTE_UNUSED, jthread* thread_ptr) {
  art::Thread* self = art::Thread::Current();

  art::ScopedObjectAccess soa(self);

  jthread thread_peer;
  if (self->IsStillStarting()) {
    thread_peer = nullptr;
  } else {
    thread_peer = soa.AddLocalReference<jthread>(self->GetPeer());
  }

  *thread_ptr = thread_peer;
  return ERR(NONE);
}

// Read the context classloader from a Java thread object. This is a lazy implementation
// that assumes GetThreadInfo isn't called too often. If we instead cache the ArtField,
// we will have to add synchronization as this can't be cached on startup (which is
// potentially runtime startup).
static art::ObjPtr<art::mirror::Object> GetContextClassLoader(art::ObjPtr<art::mirror::Object> peer)
    REQUIRES_SHARED(art::Locks::mutator_lock_) {
  art::ObjPtr<art::mirror::Class> klass = peer->GetClass();
  art::ArtField* cc_field = klass->FindDeclaredInstanceField("contextClassLoader",
                                                             "Ljava/lang/ClassLoader;");
  CHECK(cc_field != nullptr);
  return cc_field->GetObject(peer);
}

jvmtiError ThreadUtil::GetThreadInfo(jvmtiEnv* env, jthread thread, jvmtiThreadInfo* info_ptr) {
  art::ScopedObjectAccess soa(art::Thread::Current());

  art::Thread* self = nullptr;
  if (thread != nullptr) {
    art::MutexLock mu(soa.Self(), *art::Locks::thread_list_lock_);
    self = art::Thread::FromManagedThread(soa, thread);
  }

  std::string name;
  self->GetThreadName(name);
  jvmtiError name_result = CopyString(
      env, name.c_str(), reinterpret_cast<unsigned char**>(&info_ptr->name));
  if (name_result != ERR(NONE)) {
    return name_result;
  }
  JvmtiUniquePtr name_uptr = MakeJvmtiUniquePtr(env, info_ptr->name);

  info_ptr->priority = self->GetNativePriority();

  info_ptr->is_daemon = self->IsDaemon();

  art::ObjPtr<art::mirror::Object> peer = self->GetPeer();
  if (peer != nullptr) {
    art::ArtField* f = art::jni::DecodeArtField(art::WellKnownClasses::java_lang_Thread_group);
    CHECK(f != nullptr);
    art::ObjPtr<art::mirror::Object> group = f->GetObject(peer);
    info_ptr->thread_group = soa.AddLocalReference<jthreadGroup>(group);
    info_ptr->context_class_loader = soa.AddLocalReference<jobject>(GetContextClassLoader(peer));
  } else {
    info_ptr->thread_group = nullptr;
    info_ptr->context_class_loader = nullptr;
  }

  name_uptr.release();

  return ERR(NONE);
}

}  // namespace openjdkjvmti
