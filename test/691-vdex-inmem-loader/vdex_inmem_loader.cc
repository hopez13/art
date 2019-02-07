/*
 * Copyright (C) 2017 The Android Open Source Project
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

#include "class_loader_utils.h"
#include "jni.h"
#include "oat_file_assistant.h"
#include "oat_file_manager.h"
#include "scoped_thread_state_change-inl.h"
#include "thread.h"

namespace art {
namespace Test691VdexInmemLoader {

extern "C" JNIEXPORT void JNICALL Java_Main_setProcessDataDir(JNIEnv* env, jclass, jstring jpath) {
  const char* path = env->GetStringUTFChars(jpath, nullptr);
  Runtime::Current()->SetProcessDataDirectory(path);
  env->ReleaseStringUTFChars(jpath, path);
}

extern "C" JNIEXPORT bool JNICALL Java_Main_hasVdexFile(JNIEnv*,
                                                        jclass,
                                                        jobject loader) {
  ScopedObjectAccess soa(Thread::Current());
  StackHandleScope<1> hs(soa.Self());
  Handle<mirror::ClassLoader> h_loader = hs.NewHandle(soa.Decode<mirror::ClassLoader>(loader));

  bool is_first = true;
  bool all_vdex_exists = false;

  VisitClassLoaderDexFiles(
      soa,
      h_loader,
      [&](const DexFile* dex_file) {
        std::vector<const DexFile::Header*> dex_headers;
        dex_headers.push_back(&dex_file->GetHeader());

        std::string dex_location;
        std::string vdex_filename;
        std::string error_msg;
        bool vdex_exists =
            OatFileAssistant::DexFilesToAnonymousDexLocation(dex_headers, &dex_location) &&
            OatFileAssistant::DexLocationToVdexFilename(dex_location,
                                                        kRuntimeISA,
                                                        &vdex_filename,
                                                        &error_msg) &&
            OS::FileExists(vdex_filename.c_str());

        if (is_first) {
          all_vdex_exists = vdex_exists;
          is_first = false;
        } else {
          // DexFiles should either all or none have a vdex.
          CHECK_EQ(all_vdex_exists, vdex_exists);
        }
        return true;
      });

  return all_vdex_exists;
}

extern "C" JNIEXPORT jboolean JNICALL Java_Main_isBackedByOatFile(JNIEnv*,
                                                                  jclass,
                                                                  jobject loader) {
  ScopedObjectAccess soa(Thread::Current());
  StackHandleScope<1> hs(soa.Self());
  Handle<mirror::ClassLoader> h_loader = hs.NewHandle(soa.Decode<mirror::ClassLoader>(loader));

  bool is_first = true;
  bool all_backed_by_oat = false;

  VisitClassLoaderDexFiles(soa,
                           h_loader,
                           [&](const DexFile* dex_file) {
                             bool is_backed_by_oat = (dex_file->GetOatDexFile() != nullptr);
                             if (is_first) {
                               all_backed_by_oat = is_backed_by_oat;
                               is_first = false;
                             } else {
                               // DexFiles should either all or none be backed by oat.
                               CHECK_EQ(all_backed_by_oat, is_backed_by_oat);
                             }
                             return true;
                           });
  return all_backed_by_oat ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT jboolean JNICALL Java_Main_areClassesPreverified(JNIEnv*,
                                                                      jclass,
                                                                      jobject loader) {
  ScopedObjectAccess soa(Thread::Current());
  StackHandleScope<1> hs(soa.Self());
  Handle<mirror::ClassLoader> h_loader = hs.NewHandle(soa.Decode<mirror::ClassLoader>(loader));
  ClassLinker* const class_linker = Runtime::Current()->GetClassLinker();

  bool is_first = true;
  bool all_preverified = false;

  std::vector<const DexFile::Header*> dex_headers;
  VisitClassLoaderDexFiles(
      soa,
      h_loader,
      [&](const DexFile* dex_file) REQUIRES_SHARED(Locks::mutator_lock_) {
        StackHandleScope<1> hs2(soa.Self());
        for (uint16_t cdef_idx = 0; cdef_idx < dex_file->NumClassDefs(); ++cdef_idx) {
          const char* desc = dex_file->GetClassDescriptor(dex_file->GetClassDef(cdef_idx));
          Handle<mirror::Class> h_class(
              hs2.NewHandle(class_linker->FindClass(soa.Self(), desc, h_loader)));
          CHECK(h_class != nullptr) << "Could not find class " << desc;

          ClassStatus oat_file_class_status(ClassStatus::kNotReady);
          bool is_preverified = class_linker->VerifyClassUsingOatFile(
              *dex_file, h_class.Get(), oat_file_class_status);

          if (is_first) {
            all_preverified = is_preverified;
            is_first = false;
          } else {
            // Classes should either all or none be preverified.
            CHECK_EQ(all_preverified, is_preverified);
          }
        }
        return true;
      });
  return all_preverified ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT jint JNICALL Java_Main_getVdexCacheSize(JNIEnv*, jclass) {
  return static_cast<jint>(OatFileManager::kInMemoryDexClassLoaderCacheSize);
}

}  // namespace Test691VdexInmemLoader
}  // namespace art