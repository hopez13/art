/*
 * Copyright (C) 2016 The Android Open Source Project
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

#include "verifier_deps.h"

#include "class_linker.h"
#include "common_runtime_test.h"
#include "compiler_callbacks.h"
#include "dex_file.h"
#include "handle_scope-inl.h"
#include "method_verifier-inl.h"
#include "mirror/class_loader.h"
#include "runtime.h"
#include "thread.h"
#include "scoped_thread_state_change.h"

namespace art {
namespace verifier {

class VerifierDepsCompilerCallbacks : public CompilerCallbacks {
 public:
  explicit VerifierDepsCompilerCallbacks(verifier::VerifierDeps* deps)
      : CompilerCallbacks(CompilerCallbacks::CallbackMode::kCompileApp),
        deps_(deps) {}

  void MethodVerified(verifier::MethodVerifier* verifier ATTRIBUTE_UNUSED) OVERRIDE {}
  void ClassRejected(ClassReference ref ATTRIBUTE_UNUSED) OVERRIDE {}
  bool IsRelocationPossible() OVERRIDE { return false; }

  verifier::VerifierDeps* GetVerifierDeps() const OVERRIDE { return deps_; }

 private:
  verifier::VerifierDeps* deps_;
};

class VerifierDepsTest : public CommonRuntimeTest {
 public:
  void SetUpRuntimeOptions(RuntimeOptions* options) {
    CommonRuntimeTest::SetUpRuntimeOptions(options);

    verifier_deps_.reset(new VerifierDeps());
    callbacks_.reset(new VerifierDepsCompilerCallbacks(verifier_deps_.get()));
  }

  mirror::Class* FindClassByName(const std::string& name, ScopedObjectAccess* soa)
      REQUIRES_SHARED(Locks::mutator_lock_) {
    StackHandleScope<1> hs(Thread::Current());
    Handle<mirror::ClassLoader> class_loader_handle(
        hs.NewHandle(soa->Decode<mirror::ClassLoader*>(class_loader_)));
    mirror::Class* result = class_linker_->FindClass(Thread::Current(),
                                                     name.c_str(),
                                                     class_loader_handle);
    DCHECK(result != nullptr) << name;
    return result;
  }

  void LoadDexFile(ScopedObjectAccess* soa) REQUIRES_SHARED(Locks::mutator_lock_) {
    class_loader_ = LoadDex("VerifierDeps");
    std::vector<const DexFile*> dex_files = GetDexFiles(class_loader_);
    CHECK_EQ(dex_files.size(), 1u);
    dex_file_ = dex_files.front();

    mirror::ClassLoader* loader = soa->Decode<mirror::ClassLoader*>(class_loader_);
    class_linker_ = Runtime::Current()->GetClassLinker();
    class_linker_->RegisterDexFile(*dex_file_, loader);

    klass_Main_ = FindClassByName("LMain;", soa);
    CHECK(klass_Main_ != nullptr);

    verifier_deps_->AddCompiledDexFile(*dex_file_);
  }

  bool VerifyMethod(const std::string& method_name) {
    ScopedObjectAccess soa(Thread::Current());
    LoadDexFile(&soa);

    StackHandleScope<2> hs(Thread::Current());
    Handle<mirror::ClassLoader> class_loader_handle(
        hs.NewHandle(soa.Decode<mirror::ClassLoader*>(class_loader_)));
    Handle<mirror::DexCache> dex_cache_handle(hs.NewHandle(klass_Main_->GetDexCache()));

    const DexFile::ClassDef* class_def = klass_Main_->GetClassDef();
    const uint8_t* class_data = dex_file_->GetClassData(*class_def);
    CHECK(class_data != nullptr);

    ClassDataItemIterator it(*dex_file_, class_data);
    while (it.HasNextStaticField() || it.HasNextInstanceField()) {
      it.Next();
    }

    ArtMethod* method = nullptr;
    while (it.HasNextDirectMethod()) {
      ArtMethod* resolved_method = class_linker_->ResolveMethod<ClassLinker::kNoICCECheckForCache>(
          *dex_file_,
          it.GetMemberIndex(),
          dex_cache_handle,
          class_loader_handle,
          nullptr,
          it.GetMethodInvokeType(*class_def));
      CHECK(resolved_method != nullptr);
      if (method_name == resolved_method->GetName()) {
        method = resolved_method;
        break;
      }
      it.Next();
    }
    CHECK(method != nullptr);

    MethodVerifier verifier(Thread::Current(),
                            dex_file_,
                            dex_cache_handle,
                            class_loader_handle,
                            *class_def,
                            it.GetMethodCodeItem(),
                            it.GetMemberIndex(),
                            method,
                            it.GetMethodAccessFlags(),
                            true /* can_load_classes */,
                            true /* allow_soft_failures */,
                            true /* need_precise_constants */,
                            false /* verify to dump */,
                            true /* allow_thread_suspension */);
    verifier.Verify();
    return !verifier.HasFailures();
  }

  bool TestAssignabilityRecording(const std::string& dst,
                                  const std::string& src,
                                  bool is_strict,
                                  bool is_assignable) {
    ScopedObjectAccess soa(Thread::Current());
    LoadDexFile(&soa);
    verifier_deps_->AddAssignability(*dex_file_,
                                     FindClassByName(dst, &soa),
                                     FindClassByName(src, &soa),
                                     is_strict,
                                     is_assignable);
    return true;
  }

  bool HasAssignable(const std::string& destination, const std::string& source, bool assignable) {
    MutexLock mu(Thread::Current(), *Locks::verifier_deps_lock_);
    for (auto& dex_dep : verifier_deps_->dex_deps_) {
      const DexFile& dex_file = *dex_dep.first;
      auto& storage = assignable ? dex_dep.second->assignable_types_
                                 : dex_dep.second->unassignable_types_;
      for (auto& entry : storage) {
        std::string entry_dest = verifier_deps_->GetStringFromId(dex_file, entry.GetDestination());
        std::string entry_src = verifier_deps_->GetStringFromId(dex_file, entry.GetSource());
        if ((destination == entry_dest) && (source == entry_src)) {
          return true;
        }
      }
    }
    return false;
  }

  bool HasClass(const std::string& klass, const std::string& access_flags) {
    bool want_resolved = (access_flags != "unresolved");
    MutexLock mu(Thread::Current(), *Locks::verifier_deps_lock_);
    for (auto& dex_dep : verifier_deps_->dex_deps_) {
      for (auto& entry : dex_dep.second->classes_) {
        if (want_resolved != entry.IsResolved()) {
          continue;
        }

        std::string entry_klass = dex_dep.first->StringByTypeIdx(entry.GetDexTypeIndex());
        if (klass != entry_klass) {
          continue;
        }

        if (!entry.IsResolved()) {
          return true;
        }

        std::string entry_access_flags = PrettyJavaAccessFlags(entry.GetAccessFlags());
        if (access_flags + " " != entry_access_flags) {
          continue;
        }

        return true;
      }
    }
    return false;
  }

  bool HasField(const std::string& klass,
                const std::string& name,
                const std::string& type,
                const std::string& access_flags,
                const std::string& decl_klass = "") {
    bool want_resolved = (access_flags != "unresolved");
    MutexLock mu(Thread::Current(), *Locks::verifier_deps_lock_);
    for (auto& dex_dep : verifier_deps_->dex_deps_) {
      for (auto& entry : dex_dep.second->fields_) {
        if (want_resolved != entry.IsResolved()) {
          continue;
        }

        const DexFile::FieldId& field_id = dex_dep.first->GetFieldId(entry.GetDexFieldIndex());

        std::string entry_klass = dex_dep.first->StringByTypeIdx(field_id.class_idx_);
        if (klass != entry_klass) {
          continue;
        }

        std::string entry_name = dex_dep.first->StringDataByIdx(field_id.name_idx_);
        if (name != entry_name) {
          continue;
        }

        std::string entry_type = dex_dep.first->StringByTypeIdx(field_id.type_idx_);
        if (type != entry_type) {
          continue;
        }

        if (!entry.IsResolved()) {
          return true;
        }

        std::string entry_access_flags = PrettyJavaAccessFlags(entry.GetAccessFlags());
        if (access_flags + " " != entry_access_flags) {
          continue;
        }

        std::string entry_decl_klass = verifier_deps_->GetStringFromId(
            *dex_dep.first, entry.GetDeclaringClassIndex());
        if (decl_klass != entry_decl_klass) {
          continue;
        }

        return true;
      }
    }
    return false;
  }

  bool HasMethod(const std::string& kind,
                 const std::string& klass,
                 const std::string& name,
                 const std::string& signature,
                 const std::string& access_flags,
                 const std::string& decl_klass = "") {
    bool want_resolved = (access_flags != "unresolved");
    MutexLock mu(Thread::Current(), *Locks::verifier_deps_lock_);
    for (auto& dex_dep : verifier_deps_->dex_deps_) {
      auto& storage = (kind == "direct") ? dex_dep.second->direct_methods_
                                         : (kind == "virtual") ? dex_dep.second->virtual_methods_
                                                               : dex_dep.second->interface_methods_;
      for (auto& entry : storage) {
        if (want_resolved != entry.IsResolved()) {
          continue;
        }

        const DexFile::MethodId& method_id = dex_dep.first->GetMethodId(entry.GetDexMethodIndex());

        std::string entry_klass = dex_dep.first->StringByTypeIdx(method_id.class_idx_);
        if (klass != entry_klass) {
          continue;
        }

        std::string entry_name = dex_dep.first->StringDataByIdx(method_id.name_idx_);
        if (name != entry_name) {
          continue;
        }

        std::string entry_signature = dex_dep.first->GetMethodSignature(method_id).ToString();
        if (signature != entry_signature) {
          continue;
        }

        if (!entry.IsResolved()) {
          return true;
        }

        std::string entry_access_flags = PrettyJavaAccessFlags(entry.GetAccessFlags());
        if (access_flags + " " != entry_access_flags) {
          continue;
        }

        std::string entry_decl_klass = verifier_deps_->GetStringFromId(
            *dex_dep.first, entry.GetDeclaringClassIndex());
        if (decl_klass != entry_decl_klass) {
          continue;
        }

        return true;
      }
    }
    return false;
  }

  std::unique_ptr<verifier::VerifierDeps> verifier_deps_;
  const DexFile* dex_file_;
  jobject class_loader_;
  ClassLinker* class_linker_;
  mirror::Class* klass_Main_;
};

TEST_F(VerifierDepsTest, StringToId) {
  ScopedObjectAccess soa(Thread::Current());
  LoadDexFile(&soa);

  MutexLock mu(Thread::Current(), *Locks::verifier_deps_lock_);

  uint32_t id_Main1 = verifier_deps_->GetIdFromString(*dex_file_, "LMain;");
  ASSERT_LT(id_Main1, dex_file_->NumStringIds());
  ASSERT_EQ("LMain;", verifier_deps_->GetStringFromId(*dex_file_, id_Main1));

  uint32_t id_Main2 = verifier_deps_->GetIdFromString(*dex_file_, "LMain;");
  ASSERT_LT(id_Main2, dex_file_->NumStringIds());
  ASSERT_EQ("LMain;", verifier_deps_->GetStringFromId(*dex_file_, id_Main2));

  uint32_t id_Lorem1 = verifier_deps_->GetIdFromString(*dex_file_, "Lorem ipsum");
  ASSERT_GE(id_Lorem1, dex_file_->NumStringIds());
  ASSERT_EQ("Lorem ipsum", verifier_deps_->GetStringFromId(*dex_file_, id_Lorem1));

  uint32_t id_Lorem2 = verifier_deps_->GetIdFromString(*dex_file_, "Lorem ipsum");
  ASSERT_GE(id_Lorem2, dex_file_->NumStringIds());
  ASSERT_EQ("Lorem ipsum", verifier_deps_->GetStringFromId(*dex_file_, id_Lorem2));

  ASSERT_EQ(id_Main1, id_Main2);
  ASSERT_EQ(id_Lorem1, id_Lorem2);
  ASSERT_NE(id_Main1, id_Lorem1);
}

TEST_F(VerifierDepsTest, Assignable_BothInBoot) {
  ASSERT_TRUE(TestAssignabilityRecording(/* dst */ "Ljava/util/TimeZone;",
                                         /* src */ "Ljava/util/SimpleTimeZone;",
                                         /* is_strict */ true,
                                         /* is_assignable */ true));
  ASSERT_TRUE(HasAssignable("Ljava/util/TimeZone;", "Ljava/util/SimpleTimeZone;", true));
}

TEST_F(VerifierDepsTest, Assignable_DestinationInBoot1) {
  ASSERT_TRUE(TestAssignabilityRecording(/* dst */ "Ljava/net/Socket;",
                                         /* src */ "LMySSLSocket;",
                                         /* is_strict */ true,
                                         /* is_assignable */ true));
  ASSERT_TRUE(HasAssignable("Ljava/net/Socket;", "LMySSLSocket;", true));
}

TEST_F(VerifierDepsTest, Assignable_DestinationInBoot2) {
  ASSERT_TRUE(TestAssignabilityRecording(/* dst */ "Ljava/util/TimeZone;",
                                         /* src */ "LMySimpleTimeZone;",
                                         /* is_strict */ true,
                                         /* is_assignable */ true));
  ASSERT_TRUE(HasAssignable("Ljava/util/TimeZone;", "LMySimpleTimeZone;", true));
}

TEST_F(VerifierDepsTest, Assignable_DestinationInBoot3) {
  ASSERT_TRUE(TestAssignabilityRecording(/* dst */ "Ljava/util/Collection;",
                                         /* src */ "LMyThreadSet;",
                                         /* is_strict */ true,
                                         /* is_assignable */ true));
  ASSERT_TRUE(HasAssignable("Ljava/util/Collection;", "LMyThreadSet;", true));
}

TEST_F(VerifierDepsTest, Assignable_BothArrays) {
  ASSERT_TRUE(TestAssignabilityRecording(/* dst */ "[Ljava/util/TimeZone;",
                                         /* src */ "[Ljava/util/SimpleTimeZone;",
                                         /* is_strict */ true,
                                         /* is_assignable */ true));
  // We want to store assignability on the principal
  ASSERT_FALSE(HasAssignable("[Ljava/util/TimeZone;", "[Ljava/util/SimpleTimeZone;", true));
  ASSERT_TRUE(HasAssignable("Ljava/util/TimeZone;", "Ljava/util/SimpleTimeZone;", true));
}

TEST_F(VerifierDepsTest, Assignable_ArrayToInterface1) {
  ASSERT_TRUE(TestAssignabilityRecording(/* dst */ "Ljava/io/Serializable;",
                                         /* src */ "[Ljava/util/TimeZone;",
                                         /* is_strict */ true,
                                         /* is_assignable */ true));
  ASSERT_TRUE(HasAssignable("Ljava/io/Serializable;", "[Ljava/util/TimeZone;", true));
}

TEST_F(VerifierDepsTest, Assignable_ArrayToInterface2) {
  ASSERT_TRUE(TestAssignabilityRecording(/* dst */ "Ljava/io/Serializable;",
                                         /* src */ "[LMyThreadSet;",
                                         /* is_strict */ true,
                                         /* is_assignable */ true));
  ASSERT_TRUE(HasAssignable("Ljava/io/Serializable;", "[LMyThreadSet;", true));
}

TEST_F(VerifierDepsTest, NotAssignable_BothInBoot) {
  ASSERT_TRUE(TestAssignabilityRecording(/* dst */ "Ljava/lang/Exception;",
                                         /* src */ "Ljava/util/SimpleTimeZone;",
                                         /* is_strict */ true,
                                         /* is_assignable */ false));
  ASSERT_TRUE(HasAssignable("Ljava/lang/Exception;", "Ljava/util/SimpleTimeZone;", false));
}

TEST_F(VerifierDepsTest, NotAssignable_DestinationInBoot1) {
  ASSERT_TRUE(TestAssignabilityRecording(/* dst */ "Ljava/lang/Exception;",
                                         /* src */ "LMySSLSocket;",
                                         /* is_strict */ true,
                                         /* is_assignable */ false));
  ASSERT_TRUE(HasAssignable("Ljava/lang/Exception;", "LMySSLSocket;", false));
}

TEST_F(VerifierDepsTest, NotAssignable_DestinationInBoot2) {
  ASSERT_TRUE(TestAssignabilityRecording(/* dst */ "Ljava/lang/Exception;",
                                         /* src */ "LMySimpleTimeZone;",
                                         /* is_strict */ true,
                                         /* is_assignable */ false));
  ASSERT_TRUE(HasAssignable("Ljava/lang/Exception;", "LMySimpleTimeZone;", false));
}

TEST_F(VerifierDepsTest, NotAssignable_BothArrays) {
  ASSERT_TRUE(TestAssignabilityRecording(/* dst */ "[Ljava/lang/Exception;",
                                         /* src */ "[Ljava/util/SimpleTimeZone;",
                                         /* is_strict */ true,
                                         /* is_assignable */ false));
  ASSERT_TRUE(HasAssignable("Ljava/lang/Exception;", "Ljava/util/SimpleTimeZone;", false));
}

TEST_F(VerifierDepsTest, ArgumentType_ResolvedClass) {
  ASSERT_TRUE(VerifyMethod("ArgumentType_ResolvedClass"));
  ASSERT_TRUE(HasClass("Ljava/lang/Thread;", "public"));
}

TEST_F(VerifierDepsTest, ArgumentType_ResolvedReferenceArray) {
  ASSERT_TRUE(VerifyMethod("ArgumentType_ResolvedReferenceArray"));
  ASSERT_TRUE(HasClass("[Ljava/lang/Thread;", "public final abstract"));
}

TEST_F(VerifierDepsTest, ArgumentType_ResolvedPrimitiveArray) {
  ASSERT_TRUE(VerifyMethod("ArgumentType_ResolvedPrimitiveArray"));
  ASSERT_TRUE(HasClass("[B", "public final abstract"));
}

TEST_F(VerifierDepsTest, ArgumentType_UnresolvedClass) {
  ASSERT_TRUE(VerifyMethod("ArgumentType_UnresolvedClass"));
  ASSERT_TRUE(HasClass("LUnresolvedClass;", "unresolved"));
}

TEST_F(VerifierDepsTest, ArgumentType_UnresolvedSuper) {
  ASSERT_TRUE(VerifyMethod("ArgumentType_UnresolvedSuper"));
  ASSERT_TRUE(HasClass("LMySetWithUnresolvedSuper;", "unresolved"));
}

TEST_F(VerifierDepsTest, ReturnType_Reference) {
  ASSERT_TRUE(VerifyMethod("ReturnType_Reference"));
  ASSERT_TRUE(HasAssignable("Ljava/lang/Throwable;", "Ljava/lang/IllegalStateException;", true));
}

TEST_F(VerifierDepsTest, ReturnType_Array) {
  ASSERT_FALSE(VerifyMethod("ReturnType_Array"));
  ASSERT_TRUE(HasAssignable("Ljava/lang/Integer;", "Ljava/lang/IllegalStateException;", false));
}

TEST_F(VerifierDepsTest, InvokeArgumentType) {
  ASSERT_TRUE(VerifyMethod("InvokeArgumentType"));
  ASSERT_TRUE(HasClass("Ljava/text/SimpleDateFormat;", "public"));
  ASSERT_TRUE(HasClass("Ljava/util/SimpleTimeZone;", "public"));
  ASSERT_TRUE(HasMethod("virtual",
                        "Ljava/text/SimpleDateFormat;",
                        "setTimeZone",
                        "(Ljava/util/TimeZone;)V",
                        "public",
                        "Ljava/text/DateFormat;"));
  ASSERT_TRUE(HasAssignable("Ljava/util/TimeZone;", "Ljava/util/SimpleTimeZone;", true));
}

TEST_F(VerifierDepsTest, MergeTypes_RegisterLines) {
  ASSERT_TRUE(VerifyMethod("MergeTypes_RegisterLines"));
  ASSERT_TRUE(HasAssignable("Ljava/lang/Exception;", "LMySocketTimeoutException;", true));
  ASSERT_TRUE(HasAssignable(
      "Ljava/lang/Exception;", "Ljava/util/concurrent/TimeoutException;", true));
}

TEST_F(VerifierDepsTest, MergeTypes_IfInstanceOf) {
  ASSERT_TRUE(VerifyMethod("MergeTypes_IfInstanceOf"));
  ASSERT_TRUE(HasAssignable("Ljava/lang/Exception;", "Ljava/net/SocketTimeoutException;", true));
  ASSERT_TRUE(HasAssignable(
      "Ljava/lang/Exception;", "Ljava/util/concurrent/TimeoutException;", true));
  ASSERT_TRUE(HasAssignable("Ljava/net/SocketTimeoutException;", "Ljava/lang/Exception;", false));
}

TEST_F(VerifierDepsTest, MergeTypes_Unresolved) {
  ASSERT_TRUE(VerifyMethod("MergeTypes_Unresolved"));
  ASSERT_TRUE(HasAssignable("Ljava/lang/Exception;", "Ljava/net/SocketTimeoutException;", true));
  ASSERT_TRUE(HasAssignable(
      "Ljava/lang/Exception;", "Ljava/util/concurrent/TimeoutException;", true));
}

TEST_F(VerifierDepsTest, ConstClass_Resolved) {
  ASSERT_TRUE(VerifyMethod("ConstClass_Resolved"));
  ASSERT_TRUE(HasClass("Ljava/lang/IllegalStateException;", "public"));
}

TEST_F(VerifierDepsTest, ConstClass_Unresolved) {
  ASSERT_TRUE(VerifyMethod("ConstClass_Unresolved"));
  ASSERT_TRUE(HasClass("LUnresolvedClass;", "unresolved"));
}

TEST_F(VerifierDepsTest, CheckCast_Resolved) {
  ASSERT_TRUE(VerifyMethod("CheckCast_Resolved"));
  ASSERT_TRUE(HasClass("Ljava/lang/IllegalStateException;", "public"));
}

TEST_F(VerifierDepsTest, CheckCast_Unresolved) {
  ASSERT_TRUE(VerifyMethod("CheckCast_Unresolved"));
  ASSERT_TRUE(HasClass("LUnresolvedClass;", "unresolved"));
}

TEST_F(VerifierDepsTest, InstanceOf_Resolved) {
  ASSERT_TRUE(VerifyMethod("InstanceOf_Resolved"));
  ASSERT_TRUE(HasClass("Ljava/lang/IllegalStateException;", "public"));
}

TEST_F(VerifierDepsTest, InstanceOf_Unresolved) {
  ASSERT_TRUE(VerifyMethod("InstanceOf_Unresolved"));
  ASSERT_TRUE(HasClass("LUnresolvedClass;", "unresolved"));
}

TEST_F(VerifierDepsTest, NewInstance_Resolved) {
  ASSERT_TRUE(VerifyMethod("NewInstance_Resolved"));
  ASSERT_TRUE(HasClass("Ljava/lang/IllegalStateException;", "public"));
}

TEST_F(VerifierDepsTest, NewInstance_Unresolved) {
  ASSERT_TRUE(VerifyMethod("NewInstance_Unresolved"));
  ASSERT_TRUE(HasClass("LUnresolvedClass;", "unresolved"));
}

TEST_F(VerifierDepsTest, NewArray_Resolved) {
  ASSERT_TRUE(VerifyMethod("NewArray_Resolved"));
  ASSERT_TRUE(HasClass("[Ljava/lang/IllegalStateException;", "public final abstract"));
}

TEST_F(VerifierDepsTest, NewArray_Unresolved) {
  ASSERT_TRUE(VerifyMethod("NewArray_Unresolved"));
  ASSERT_TRUE(HasClass("[LUnresolvedClass;", "unresolved"));
}

TEST_F(VerifierDepsTest, Throw) {
  ASSERT_TRUE(VerifyMethod("Throw"));
  ASSERT_TRUE(HasAssignable("Ljava/lang/Throwable;", "Ljava/lang/IllegalStateException;", true));
}

TEST_F(VerifierDepsTest, MoveException_Resolved) {
  ASSERT_TRUE(VerifyMethod("MoveException_Resolved"));
  ASSERT_TRUE(HasClass("Ljava/io/InterruptedIOException;", "public"));
  ASSERT_TRUE(HasClass("Ljava/net/SocketTimeoutException;", "public"));
  ASSERT_TRUE(HasClass("Ljava/util/zip/ZipException;", "public"));

  // Testing that all exception types are assignable to Throwable.
  ASSERT_TRUE(HasAssignable("Ljava/lang/Throwable;", "Ljava/io/InterruptedIOException;", true));
  ASSERT_TRUE(HasAssignable("Ljava/lang/Throwable;", "Ljava/net/SocketTimeoutException;", true));
  ASSERT_TRUE(HasAssignable("Ljava/lang/Throwable;", "Ljava/util/zip/ZipException;", true));

  // Testing that the merge type is assignable to Throwable.
  ASSERT_TRUE(HasAssignable("Ljava/lang/Throwable;", "Ljava/io/IOException;", true));

  // Merging of exception types.
  ASSERT_TRUE(HasAssignable("Ljava/io/IOException;", "Ljava/io/InterruptedIOException;", true));
  ASSERT_TRUE(HasAssignable("Ljava/io/IOException;", "Ljava/util/zip/ZipException;", true));
  ASSERT_TRUE(HasAssignable(
      "Ljava/io/InterruptedIOException;", "Ljava/net/SocketTimeoutException;", true));
}

TEST_F(VerifierDepsTest, MoveException_Unresolved) {
  ASSERT_FALSE(VerifyMethod("MoveException_Unresolved"));
  ASSERT_TRUE(HasClass("LUnresolvedException;", "unresolved"));
}

TEST_F(VerifierDepsTest, StaticField_Resolved_DeclaredInReferenced) {
  ASSERT_TRUE(VerifyMethod("StaticField_Resolved_DeclaredInReferenced"));
  ASSERT_TRUE(HasClass("Ljava/lang/System;", "public final"));
  ASSERT_TRUE(HasField("Ljava/lang/System;",
                       "out",
                       "Ljava/io/PrintStream;",
                       "public final static",
                       "Ljava/lang/System;"));
}

TEST_F(VerifierDepsTest, StaticField_Resolved_DeclaredInSuperclass1) {
  ASSERT_TRUE(VerifyMethod("StaticField_Resolved_DeclaredInSuperclass1"));
  ASSERT_TRUE(HasClass("Ljava/util/SimpleTimeZone;", "public"));
  ASSERT_TRUE(HasField(
      "Ljava/util/SimpleTimeZone;", "LONG", "I", "public final static", "Ljava/util/TimeZone;"));
}

TEST_F(VerifierDepsTest, StaticField_Resolved_DeclaredInSuperclass2) {
  ASSERT_TRUE(VerifyMethod("StaticField_Resolved_DeclaredInSuperclass2"));
  ASSERT_TRUE(HasField(
      "LMySimpleTimeZone;", "SHORT", "I", "public final static", "Ljava/util/TimeZone;"));
}

TEST_F(VerifierDepsTest, StaticField_Resolved_DeclaredInInterface1) {
  ASSERT_TRUE(VerifyMethod("StaticField_Resolved_DeclaredInInterface1"));
  ASSERT_TRUE(HasClass("Ljavax/xml/transform/dom/DOMResult;", "public"));
  ASSERT_TRUE(HasField("Ljavax/xml/transform/dom/DOMResult;",
                       "PI_ENABLE_OUTPUT_ESCAPING",
                       "Ljava/lang/String;",
                       "public final static",
                       "Ljavax/xml/transform/Result;"));
}

TEST_F(VerifierDepsTest, StaticField_Resolved_DeclaredInInterface2) {
  ASSERT_TRUE(VerifyMethod("StaticField_Resolved_DeclaredInInterface2"));
  ASSERT_TRUE(HasField("LMyDOMResult;",
                       "PI_ENABLE_OUTPUT_ESCAPING",
                       "Ljava/lang/String;",
                       "public final static",
                       "Ljavax/xml/transform/Result;"));
}

TEST_F(VerifierDepsTest, StaticField_Resolved_DeclaredInInterface3) {
  ASSERT_TRUE(VerifyMethod("StaticField_Resolved_DeclaredInInterface3"));
  ASSERT_TRUE(HasField("LMyResult;",
                       "PI_ENABLE_OUTPUT_ESCAPING",
                       "Ljava/lang/String;",
                       "public final static",
                       "Ljavax/xml/transform/Result;"));
}

TEST_F(VerifierDepsTest, StaticField_Resolved_DeclaredInInterface4) {
  ASSERT_TRUE(VerifyMethod("StaticField_Resolved_DeclaredInInterface4"));
  ASSERT_TRUE(HasField("LMyDocument;",
                       "ELEMENT_NODE",
                       "S",
                       "public final static",
                       "Lorg/w3c/dom/Node;"));
}

TEST_F(VerifierDepsTest, StaticField_Unresolved_ReferrerInBoot) {
  ASSERT_TRUE(VerifyMethod("StaticField_Unresolved_ReferrerInBoot"));
  ASSERT_TRUE(HasClass("Ljava/util/TimeZone;", "public abstract"));
  ASSERT_TRUE(HasField("Ljava/util/TimeZone;", "x", "I", "unresolved"));
}

TEST_F(VerifierDepsTest, StaticField_Unresolved_ReferrerInDex) {
  ASSERT_TRUE(VerifyMethod("StaticField_Unresolved_ReferrerInDex"));
  ASSERT_TRUE(HasField("LMyThreadSet;", "x", "I", "unresolved"));
}

TEST_F(VerifierDepsTest, InstanceField_Resolved_DeclaredInReferenced) {
  ASSERT_TRUE(VerifyMethod("InstanceField_Resolved_DeclaredInReferenced"));
  ASSERT_TRUE(HasClass("Ljava/io/InterruptedIOException;", "public"));
  ASSERT_TRUE(HasField("Ljava/io/InterruptedIOException;",
                       "bytesTransferred",
                       "I",
                       "public",
                       "Ljava/io/InterruptedIOException;"));
  ASSERT_TRUE(HasAssignable(
      "Ljava/io/InterruptedIOException;", "LMySocketTimeoutException;", true));
}

TEST_F(VerifierDepsTest, InstanceField_Resolved_DeclaredInSuperclass1) {
  ASSERT_TRUE(VerifyMethod("InstanceField_Resolved_DeclaredInSuperclass1"));
  ASSERT_TRUE(HasClass("Ljava/net/SocketTimeoutException;", "public"));
  ASSERT_TRUE(HasField("Ljava/net/SocketTimeoutException;",
                       "bytesTransferred",
                       "I",
                       "public",
                       "Ljava/io/InterruptedIOException;"));
  ASSERT_TRUE(HasAssignable(
      "Ljava/io/InterruptedIOException;", "LMySocketTimeoutException;", true));
}

TEST_F(VerifierDepsTest, InstanceField_Resolved_DeclaredInSuperclass2) {
  ASSERT_TRUE(VerifyMethod("InstanceField_Resolved_DeclaredInSuperclass2"));
  ASSERT_TRUE(HasField("LMySocketTimeoutException;",
                       "bytesTransferred",
                       "I",
                       "public",
                       "Ljava/io/InterruptedIOException;"));
  ASSERT_TRUE(HasAssignable(
      "Ljava/io/InterruptedIOException;", "LMySocketTimeoutException;", true));
}

TEST_F(VerifierDepsTest, InstanceField_Unresolved_ReferrerInBoot) {
  ASSERT_TRUE(VerifyMethod("InstanceField_Unresolved_ReferrerInBoot"));
  ASSERT_TRUE(HasClass("Ljava/io/InterruptedIOException;", "public"));
  ASSERT_TRUE(HasField("Ljava/io/InterruptedIOException;", "x", "I", "unresolved"));
}

TEST_F(VerifierDepsTest, InstanceField_Unresolved_ReferrerInDex) {
  ASSERT_TRUE(VerifyMethod("InstanceField_Unresolved_ReferrerInDex"));
  ASSERT_TRUE(HasField("LMyThreadSet;", "x", "I", "unresolved"));
}

TEST_F(VerifierDepsTest, InvokeStatic_Resolved_DeclaredInReferenced) {
  ASSERT_TRUE(VerifyMethod("InvokeStatic_Resolved_DeclaredInReferenced"));
  ASSERT_TRUE(HasClass("Ljava/net/Socket;", "public"));
  ASSERT_TRUE(HasMethod("direct",
                        "Ljava/net/Socket;",
                        "setSocketImplFactory",
                        "(Ljava/net/SocketImplFactory;)V",
                        "public static",
                        "Ljava/net/Socket;"));
}

TEST_F(VerifierDepsTest, InvokeStatic_Resolved_DeclaredInSuperclass1) {
  ASSERT_TRUE(VerifyMethod("InvokeStatic_Resolved_DeclaredInSuperclass1"));
  ASSERT_TRUE(HasClass("Ljavax/net/ssl/SSLSocket;", "public abstract"));
  ASSERT_TRUE(HasMethod("direct",
                        "Ljavax/net/ssl/SSLSocket;",
                        "setSocketImplFactory",
                        "(Ljava/net/SocketImplFactory;)V",
                        "public static",
                        "Ljava/net/Socket;"));
}

TEST_F(VerifierDepsTest, InvokeStatic_Resolved_DeclaredInSuperclass2) {
  ASSERT_TRUE(VerifyMethod("InvokeStatic_Resolved_DeclaredInSuperclass2"));
  ASSERT_TRUE(HasMethod("direct",
                        "LMySSLSocket;",
                        "setSocketImplFactory",
                        "(Ljava/net/SocketImplFactory;)V",
                        "public static",
                        "Ljava/net/Socket;"));
}

TEST_F(VerifierDepsTest, InvokeStatic_DeclaredInInterface1) {
  ASSERT_TRUE(VerifyMethod("InvokeStatic_DeclaredInInterface1"));
  ASSERT_TRUE(HasClass("Ljava/util/Map$Entry;", "public abstract interface"));
  ASSERT_TRUE(HasMethod("direct",
                        "Ljava/util/Map$Entry;",
                        "comparingByKey",
                        "()Ljava/util/Comparator;",
                        "public static",
                        "Ljava/util/Map$Entry;"));
}

TEST_F(VerifierDepsTest, InvokeStatic_DeclaredInInterface2) {
  ASSERT_FALSE(VerifyMethod("InvokeStatic_DeclaredInInterface2"));
  ASSERT_TRUE(HasClass("Ljava/util/AbstractMap$SimpleEntry;", "public"));
  ASSERT_TRUE(HasMethod("direct",
                        "Ljava/util/AbstractMap$SimpleEntry;",
                        "comparingByKey",
                        "()Ljava/util/Comparator;",
                        "unresolved"));
}

TEST_F(VerifierDepsTest, InvokeStatic_Unresolved1) {
  ASSERT_FALSE(VerifyMethod("InvokeStatic_Unresolved1"));
  ASSERT_TRUE(HasClass("Ljavax/net/ssl/SSLSocket;", "public abstract"));
  ASSERT_TRUE(HasMethod("direct", "Ljavax/net/ssl/SSLSocket;", "x", "()V", "unresolved"));
}

TEST_F(VerifierDepsTest, InvokeStatic_Unresolved2) {
  ASSERT_FALSE(VerifyMethod("InvokeStatic_Unresolved2"));
  ASSERT_TRUE(HasMethod("direct", "LMySSLSocket;", "x", "()V", "unresolved"));
}

TEST_F(VerifierDepsTest, InvokeDirect_Resolved_DeclaredInReferenced) {
  ASSERT_TRUE(VerifyMethod("InvokeDirect_Resolved_DeclaredInReferenced"));
  ASSERT_TRUE(HasClass("Ljava/net/Socket;", "public"));
  ASSERT_TRUE(HasMethod(
      "direct", "Ljava/net/Socket;", "<init>", "()V", "public", "Ljava/net/Socket;"));
}

TEST_F(VerifierDepsTest, InvokeDirect_Resolved_DeclaredInSuperclass1) {
  ASSERT_FALSE(VerifyMethod("InvokeDirect_Resolved_DeclaredInSuperclass1"));
  ASSERT_TRUE(HasClass("Ljavax/net/ssl/SSLSocket;", "public abstract"));
  ASSERT_TRUE(HasMethod("direct",
                        "Ljavax/net/ssl/SSLSocket;",
                        "checkOldImpl",
                        "()V",
                        "private",
                        "Ljava/net/Socket;"));
}

TEST_F(VerifierDepsTest, InvokeDirect_Resolved_DeclaredInSuperclass2) {
  ASSERT_FALSE(VerifyMethod("InvokeDirect_Resolved_DeclaredInSuperclass2"));
  ASSERT_TRUE(HasMethod(
      "direct", "LMySSLSocket;", "checkOldImpl", "()V", "private", "Ljava/net/Socket;"));
}

TEST_F(VerifierDepsTest, InvokeDirect_Unresolved1) {
  ASSERT_FALSE(VerifyMethod("InvokeDirect_Unresolved1"));
  ASSERT_TRUE(HasClass("Ljavax/net/ssl/SSLSocket;", "public abstract"));
  ASSERT_TRUE(HasMethod("direct", "Ljavax/net/ssl/SSLSocket;", "x", "()V", "unresolved"));
}

TEST_F(VerifierDepsTest, InvokeDirect_Unresolved2) {
  ASSERT_FALSE(VerifyMethod("InvokeDirect_Unresolved2"));
  ASSERT_TRUE(HasMethod("direct", "LMySSLSocket;", "x", "()V", "unresolved"));
}

TEST_F(VerifierDepsTest, InvokeVirtual_Resolved_DeclaredInReferenced) {
  ASSERT_TRUE(VerifyMethod("InvokeVirtual_Resolved_DeclaredInReferenced"));
  ASSERT_TRUE(HasClass("Ljava/lang/Throwable;", "public"));
  ASSERT_TRUE(HasMethod("virtual",
                        "Ljava/lang/Throwable;",
                        "getMessage",
                        "()Ljava/lang/String;",
                        "public",
                        "Ljava/lang/Throwable;"));
  // Type dependency on `this` argument.
  ASSERT_TRUE(HasAssignable("Ljava/lang/Throwable;", "LMySocketTimeoutException;", true));
}

TEST_F(VerifierDepsTest, InvokeVirtual_Resolved_DeclaredInSuperclass1) {
  ASSERT_TRUE(VerifyMethod("InvokeVirtual_Resolved_DeclaredInSuperclass1"));
  ASSERT_TRUE(HasClass("Ljava/io/InterruptedIOException;", "public"));
  ASSERT_TRUE(HasMethod("virtual",
                        "Ljava/io/InterruptedIOException;",
                        "getMessage",
                        "()Ljava/lang/String;",
                        "public",
                        "Ljava/lang/Throwable;"));
  // Type dependency on `this` argument.
  ASSERT_TRUE(HasAssignable("Ljava/lang/Throwable;", "LMySocketTimeoutException;", true));
}

TEST_F(VerifierDepsTest, InvokeVirtual_Resolved_DeclaredInSuperclass2) {
  ASSERT_TRUE(VerifyMethod("InvokeVirtual_Resolved_DeclaredInSuperclass2"));
  ASSERT_TRUE(HasMethod("virtual",
                        "LMySocketTimeoutException;",
                        "getMessage",
                        "()Ljava/lang/String;",
                        "public",
                        "Ljava/lang/Throwable;"));
}

TEST_F(VerifierDepsTest, InvokeVirtual_Resolved_DeclaredInSuperinterface) {
  ASSERT_TRUE(VerifyMethod("InvokeVirtual_Resolved_DeclaredInSuperinterface"));
  ASSERT_TRUE(HasMethod("virtual",
                        "LMyThreadSet;",
                        "size",
                        "()I",
                        "public abstract",
                        "Ljava/util/Set;"));
}

TEST_F(VerifierDepsTest, InvokeVirtual_Unresolved1) {
  ASSERT_FALSE(VerifyMethod("InvokeVirtual_Unresolved1"));
  ASSERT_TRUE(HasClass("Ljava/io/InterruptedIOException;", "public"));
  ASSERT_TRUE(HasMethod("virtual", "Ljava/io/InterruptedIOException;", "x", "()V", "unresolved"));
}

TEST_F(VerifierDepsTest, InvokeVirtual_Unresolved2) {
  ASSERT_FALSE(VerifyMethod("InvokeVirtual_Unresolved2"));
  ASSERT_TRUE(HasMethod("virtual", "LMySocketTimeoutException;", "x", "()V", "unresolved"));
}

TEST_F(VerifierDepsTest, InvokeVirtual_ActuallyDirect) {
  ASSERT_FALSE(VerifyMethod("InvokeVirtual_ActuallyDirect"));
  ASSERT_TRUE(HasMethod("virtual", "LMyThread;", "activeCount", "()I", "unresolved"));
  ASSERT_TRUE(HasMethod("direct",
                        "LMyThread;",
                        "activeCount",
                        "()I",
                        "public static",
                        "Ljava/lang/Thread;"));
}

TEST_F(VerifierDepsTest, InvokeInterface_Resolved_DeclaredInReferenced) {
  ASSERT_TRUE(VerifyMethod("InvokeInterface_Resolved_DeclaredInReferenced"));
  ASSERT_TRUE(HasClass("Ljava/lang/Runnable;", "public abstract interface"));
  ASSERT_TRUE(HasMethod("interface",
                        "Ljava/lang/Runnable;",
                        "run",
                        "()V",
                        "public abstract",
                        "Ljava/lang/Runnable;"));
}

TEST_F(VerifierDepsTest, InvokeInterface_Resolved_DeclaredInSuperclass) {
  ASSERT_FALSE(VerifyMethod("InvokeInterface_Resolved_DeclaredInSuperclass"));
  ASSERT_TRUE(HasMethod("interface", "LMyThread;", "join", "()V", "unresolved"));
}

TEST_F(VerifierDepsTest, InvokeInterface_Resolved_DeclaredInSuperinterface1) {
  ASSERT_FALSE(VerifyMethod("InvokeInterface_Resolved_DeclaredInSuperinterface1"));
  ASSERT_TRUE(HasMethod("interface",
                        "LMyThreadSet;",
                        "run",
                        "()V",
                        "public abstract",
                        "Ljava/lang/Runnable;"));
}

TEST_F(VerifierDepsTest, InvokeInterface_Resolved_DeclaredInSuperinterface2) {
  ASSERT_FALSE(VerifyMethod("InvokeInterface_Resolved_DeclaredInSuperinterface2"));
  ASSERT_TRUE(HasMethod("interface",
                        "LMyThreadSet;",
                        "isEmpty",
                        "()Z",
                        "public abstract",
                        "Ljava/util/Set;"));
}

TEST_F(VerifierDepsTest, InvokeInterface_Unresolved1) {
  ASSERT_FALSE(VerifyMethod("InvokeInterface_Unresolved1"));
  ASSERT_TRUE(HasClass("Ljava/lang/Runnable;", "public abstract interface"));
  ASSERT_TRUE(HasMethod("interface", "Ljava/lang/Runnable;", "x", "()V", "unresolved"));
}

TEST_F(VerifierDepsTest, InvokeInterface_Unresolved2) {
  ASSERT_FALSE(VerifyMethod("InvokeInterface_Unresolved2"));
  ASSERT_TRUE(HasMethod("interface", "LMyThreadSet;", "x", "()V", "unresolved"));
}

TEST_F(VerifierDepsTest, InvokeSuper_ThisAssignable) {
  ASSERT_TRUE(VerifyMethod("InvokeSuper_ThisAssignable"));
  ASSERT_TRUE(HasClass("Ljava/lang/Runnable;", "public abstract interface"));
  ASSERT_TRUE(HasAssignable("Ljava/lang/Runnable;", "LMain;", true));
  ASSERT_TRUE(HasMethod("interface",
                        "Ljava/lang/Runnable;",
                        "run",
                        "()V",
                        "public abstract",
                        "Ljava/lang/Runnable;"));
}

TEST_F(VerifierDepsTest, InvokeSuper_ThisNotAssignable) {
  ASSERT_FALSE(VerifyMethod("InvokeSuper_ThisNotAssignable"));
  ASSERT_TRUE(HasClass("Ljava/lang/Integer;", "public final"));
  ASSERT_TRUE(HasAssignable("Ljava/lang/Integer;", "LMain;", false));
  ASSERT_TRUE(HasMethod(
      "virtual", "Ljava/lang/Integer;", "intValue", "()I", "public", "Ljava/lang/Integer;"));
}

}  // namespace verifier
}  // namespace art
