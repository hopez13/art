/*
 * Copyright (C) 2011 The Android Open Source Project
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

#ifndef ART_RUNTIME_DEX_FILE_ANNOTATIONS_H_
#define ART_RUNTIME_DEX_FILE_ANNOTATIONS_H_

#include "dex_file.h"

#include "mirror/object_array.h"

namespace art {

namespace mirror {
  class ClassLoader;
  class DexCache;
}  // namespace mirror
class ArtField;
class ArtMethod;
class ClassLinker;

namespace annotations {

const DexFile::AnnotationSetItem* FindAnnotationSetForField(ArtField* field)
    REQUIRES_SHARED(Locks::mutator_lock_);
mirror::Object* GetAnnotationForField(ArtField* field, Handle<mirror::Class> annotation_class)
    REQUIRES_SHARED(Locks::mutator_lock_);
mirror::ObjectArray<mirror::Object>* GetAnnotationsForField(ArtField* field)
    REQUIRES_SHARED(Locks::mutator_lock_);
mirror::ObjectArray<mirror::String>* GetSignatureAnnotationForField(ArtField* field)
    REQUIRES_SHARED(Locks::mutator_lock_);
bool IsFieldAnnotationPresent(ArtField* field, Handle<mirror::Class> annotation_class)
    REQUIRES_SHARED(Locks::mutator_lock_);

const DexFile::AnnotationSetItem* FindAnnotationSetForMethod(ArtMethod* method)
    REQUIRES_SHARED(Locks::mutator_lock_);
const DexFile::ParameterAnnotationsItem* FindAnnotationsItemForMethod(ArtMethod* method)
    REQUIRES_SHARED(Locks::mutator_lock_);
mirror::Object* GetAnnotationDefaultValue(ArtMethod* method)
    REQUIRES_SHARED(Locks::mutator_lock_);
mirror::Object* GetAnnotationForMethod(ArtMethod* method, Handle<mirror::Class> annotation_class)
    REQUIRES_SHARED(Locks::mutator_lock_);
mirror::ObjectArray<mirror::Object>* GetAnnotationsForMethod(ArtMethod* method)
    REQUIRES_SHARED(Locks::mutator_lock_);
mirror::ObjectArray<mirror::Class>* GetExceptionTypesForMethod(ArtMethod* method)
    REQUIRES_SHARED(Locks::mutator_lock_);
mirror::ObjectArray<mirror::Object>* GetParameterAnnotations(ArtMethod* method)
    REQUIRES_SHARED(Locks::mutator_lock_);
mirror::ObjectArray<mirror::String>* GetSignatureAnnotationForMethod(ArtMethod* method)
    REQUIRES_SHARED(Locks::mutator_lock_);
bool IsMethodAnnotationPresent(ArtMethod* method, Handle<mirror::Class> annotation_class,
                               uint32_t visibility = DexFile::kDexVisibilityRuntime)
    REQUIRES_SHARED(Locks::mutator_lock_);

const DexFile::AnnotationSetItem* FindAnnotationSetForClass(Handle<mirror::Class> klass)
    REQUIRES_SHARED(Locks::mutator_lock_);
mirror::Object* GetAnnotationForClass(Handle<mirror::Class> klass,
                                      Handle<mirror::Class> annotation_class)
    REQUIRES_SHARED(Locks::mutator_lock_);
mirror::ObjectArray<mirror::Object>* GetAnnotationsForClass(Handle<mirror::Class> klass)
    REQUIRES_SHARED(Locks::mutator_lock_);
mirror::ObjectArray<mirror::Class>* GetDeclaredClasses(Handle<mirror::Class> klass)
    REQUIRES_SHARED(Locks::mutator_lock_);
mirror::Class* GetDeclaringClass(Handle<mirror::Class> klass)
    REQUIRES_SHARED(Locks::mutator_lock_);
mirror::Class* GetEnclosingClass(Handle<mirror::Class> klass)
    REQUIRES_SHARED(Locks::mutator_lock_);
mirror::Object* GetEnclosingMethod(Handle<mirror::Class> klass)
    REQUIRES_SHARED(Locks::mutator_lock_);
bool GetInnerClass(Handle<mirror::Class> klass, mirror::String** name)
    REQUIRES_SHARED(Locks::mutator_lock_);
bool GetInnerClassFlags(Handle<mirror::Class> klass, uint32_t* flags)
    REQUIRES_SHARED(Locks::mutator_lock_);
mirror::ObjectArray<mirror::String>* GetSignatureAnnotationForClass(Handle<mirror::Class> klass)
    REQUIRES_SHARED(Locks::mutator_lock_);
bool IsClassAnnotationPresent(Handle<mirror::Class> klass,
                              Handle<mirror::Class> annotation_class)
    REQUIRES_SHARED(Locks::mutator_lock_);

mirror::Object* CreateAnnotationMember(Handle<mirror::Class> klass,
                                       Handle<mirror::Class> annotation_class,
                                       const uint8_t** annotation)
    REQUIRES_SHARED(Locks::mutator_lock_);
const DexFile::AnnotationItem* GetAnnotationItemFromAnnotationSet(
    Handle<mirror::Class> klass, const DexFile::AnnotationSetItem* annotation_set,
    uint32_t visibility, Handle<mirror::Class> annotation_class)
    REQUIRES_SHARED(Locks::mutator_lock_);
mirror::Object* GetAnnotationObjectFromAnnotationSet(
    Handle<mirror::Class> klass,
    const DexFile::AnnotationSetItem* annotation_set,
    uint32_t visibility,
    Handle<mirror::Class> annotation_class)
    REQUIRES_SHARED(Locks::mutator_lock_);
mirror::Object* GetAnnotationValue(Handle<mirror::Class> klass,
                                   const DexFile::AnnotationItem* annotation_item,
                                   const char* annotation_name,
                                   Handle<mirror::Class> array_class,
                                   uint32_t expected_type)
    REQUIRES_SHARED(Locks::mutator_lock_);
mirror::ObjectArray<mirror::String>* GetSignatureValue(Handle<mirror::Class> klass,
    const DexFile::AnnotationSetItem* annotation_set)
    REQUIRES_SHARED(Locks::mutator_lock_);
mirror::ObjectArray<mirror::Class>* GetThrowsValue(Handle<mirror::Class> klass,
                                                   const DexFile::AnnotationSetItem* annotation_set)
    REQUIRES_SHARED(Locks::mutator_lock_);
mirror::ObjectArray<mirror::Object>* ProcessAnnotationSet(
    Handle<mirror::Class> klass,
    const DexFile::AnnotationSetItem* annotation_set,
    uint32_t visibility)
    REQUIRES_SHARED(Locks::mutator_lock_);
mirror::ObjectArray<mirror::Object>* ProcessAnnotationSetRefList(
    Handle<mirror::Class> klass, const DexFile::AnnotationSetRefList* set_ref_list, uint32_t size)
    REQUIRES_SHARED(Locks::mutator_lock_);
mirror::Object* ProcessEncodedAnnotation(Handle<mirror::Class> klass, const uint8_t** annotation)
    REQUIRES_SHARED(Locks::mutator_lock_);
const DexFile::AnnotationItem* SearchAnnotationSet(const DexFile& dex_file,
                                                   const DexFile::AnnotationSetItem* annotation_set,
                                                   const char* descriptor, uint32_t visibility)
    REQUIRES_SHARED(Locks::mutator_lock_);
const uint8_t* SearchEncodedAnnotation(const DexFile& dex_file,
                                       const uint8_t* annotation,
                                       const char* name)
    REQUIRES_SHARED(Locks::mutator_lock_);
bool SkipAnnotationValue(const DexFile& dex_file, const uint8_t** annotation_ptr)
    REQUIRES_SHARED(Locks::mutator_lock_);

int32_t GetLineNumFromPC(ArtMethod* method, uint32_t rel_pc)
    REQUIRES_SHARED(Locks::mutator_lock_);

bool ProcessAnnotationValue(Handle<mirror::Class> klass, const uint8_t** annotation_ptr,
                            DexFile::AnnotationValue* annotation_value,
                            Handle<mirror::Class> return_class,
                            DexFile::AnnotationResultStyle result_style)
    REQUIRES_SHARED(Locks::mutator_lock_);

class RuntimeEncodedStaticFieldValueIterator : public EncodedStaticFieldValueIterator {
 public:
  // A constructor meant to be called from runtime code.
  RuntimeEncodedStaticFieldValueIterator(const DexFile& dex_file,
                                         Handle<mirror::DexCache>* dex_cache,
                                         Handle<mirror::ClassLoader>* class_loader,
                                         ClassLinker* linker,
                                         const DexFile::ClassDef& class_def)
      REQUIRES_SHARED(Locks::mutator_lock_)
      : EncodedStaticFieldValueIterator(dex_file, class_def),
        dex_cache_(dex_cache),
        class_loader_(class_loader),
        linker_(linker) {
  }

  template<bool kTransactionActive>
  void ReadValueToField(ArtField* field) const REQUIRES_SHARED(Locks::mutator_lock_);

 private:
  Handle<mirror::DexCache>* const dex_cache_;  // Dex cache to resolve literal objects.
  Handle<mirror::ClassLoader>* const class_loader_;  // ClassLoader to resolve types.
  ClassLinker* linker_;  // Linker to resolve literal objects.
  DISALLOW_IMPLICIT_CONSTRUCTORS(RuntimeEncodedStaticFieldValueIterator);
};

}  // namespace annotations

}  // namespace art

#endif  // ART_RUNTIME_DEX_FILE_ANNOTATIONS_H_
