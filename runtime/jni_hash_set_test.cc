/*
 * Copyright (C) 2024 The Android Open Source Project
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

#include "jni_hash_set.h"

#include <memory>
#include <ostream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "android-base/logging.h"
#include "android-base/stringprintf.h"
#include "arch/instruction_set.h"
#include "art_method.h"
#include "base/array_ref.h"
#include "base/locks.h"
#include "class_linker.h"
#include "common_compiler_test.h"
#include "common_compiler_test.cc"
#include "compiler.h"
#include "gc/heap.h"
#include "gc/space/image_space.h"
#include "gtest/gtest.h"
#include "handle.h"
#include "handle_scope.h"
#include "handle_scope-inl.h"
#include "image.h"
#include "image-inl.h"
#include "jni.h"
#include "mirror/class.h"
#include "mirror/class_loader.h"
#include "mirror/dex_cache.h"
#include "obj_ptr.h"
#include "runtime.h"
#include "scoped_thread_state_change.h"
#include "strstream"

namespace art {

void PrintTo(const ArrayRef<const uint8_t>& array, std::ostream* os) {
  *os << "[[[";
  for (const uint8_t& element : array) {
    *os << " ";
    *os << std::setfill('0') << std::setw(2) << std::hex << (int)element;
  }
  *os << " ]]]";
}

class JniHashSetTest : public CommonCompilerTest {
 protected:
  JniHashSetTest() : jni_hash_set_(JniShortyHash(kRuntimeISA), JniShortyEquals(kRuntimeISA)) {
    if (kRuntimeISA == InstructionSet::kArm64 || kRuntimeISA == InstructionSet::kX86_64) {
      // Only arm64 and x86_64 use strict check.
      loose_check_ = false;
    } else {
      // Other archs use loose check.
      loose_check_ = true;
    }
  }

  void SetLooseCheck(bool value) {
    loose_check_ = value;
  }

  void SetUpForTest(const char* method_name, const char* method_sig) {
    ScopedObjectAccess soa(Thread::Current());
    jobject jclass_loader = LoadDex("MyClassNatives");
    StackHandleScope<1> hs(soa.Self());
    Handle<mirror::ClassLoader> class_loader(
        hs.NewHandle(soa.Decode<mirror::ClassLoader>(jclass_loader)));
    ObjPtr<mirror::Class> klass =
        class_linker_->FindClass(soa.Self(), "LMyClassNatives;", class_loader);
    ASSERT_TRUE(klass != nullptr);
    jklass_ = soa.AddLocalReference<jclass>(klass);
    const auto pointer_size = class_linker_->GetImagePointerSize();
    method_ = klass->FindClassMethod(method_name, method_sig, pointer_size);
    ASSERT_TRUE(method_ != nullptr);
    ASSERT_TRUE(method_->IsNative());
    jni_hash_set_.insert(JniHashedKey{method_});
  }

  void CompareMethod(const char* cmp_method_name, const char* cmp_method_sig) {
    // testing::internal::CaptureStdout();
    Thread* self = Thread::Current();
    ScopedObjectAccess soa(self);
    ObjPtr<mirror::Class> klass = soa.Decode<mirror::Class>(jklass_);
    const auto pointer_size = class_linker_->GetImagePointerSize();
    ArtMethod* cmp_method = klass->FindClassMethod(cmp_method_name, cmp_method_sig, pointer_size);
    ASSERT_TRUE(cmp_method != nullptr);
    ASSERT_TRUE(cmp_method->IsNative());

    OneCompiledMethodStorage storage_method;
    OneCompiledMethodStorage storage_cmp_method;
    StackHandleScope<2> hs(self);
    std::unique_ptr<Compiler> compiler_method(
        Compiler::Create(*compiler_options_, &storage_method, compiler_kind_));
    std::unique_ptr<Compiler> compiler_cmp_method(
        Compiler::Create(*compiler_options_, &storage_cmp_method, compiler_kind_));
    const DexFile& dex_file_method = *method_->GetDexFile();
    Handle<mirror::DexCache> dex_cache_method =
        hs.NewHandle(GetClassLinker()->FindDexCache(self, dex_file_method));
    const DexFile& dex_file_cmp_method = *cmp_method->GetDexFile();
    Handle<mirror::DexCache> dex_cache_cmp_method =
        hs.NewHandle(GetClassLinker()->FindDexCache(self, dex_file_cmp_method));
    compiler_method->JniCompile(method_->GetAccessFlags(),
                         method_->GetDexMethodIndex(),
                         dex_file_method,
                         dex_cache_method);
    compiler_cmp_method->JniCompile(cmp_method->GetAccessFlags(),
                         cmp_method->GetDexMethodIndex(),
                         dex_file_cmp_method,
                         dex_cache_cmp_method);
    ArrayRef<const uint8_t> method_code = storage_method.GetCode();
    ArrayRef<const uint8_t> cmp_method_code = storage_cmp_method.GetCode();
    auto it = jni_hash_set_.find(JniHashedKey{cmp_method});
    if (it != jni_hash_set_.end()) {
      // Loose check only check equals.
      ASSERT_EQ(method_code, cmp_method_code)
          << "method: " << method_->PrettyMethod() << ", compared method: "
          << cmp_method->PrettyMethod();
    } else if (!loose_check_){
      ASSERT_NE(method_code, cmp_method_code)
          << "method: " << method_->PrettyMethod() << ", compared method: "
          << cmp_method->PrettyMethod();
    }
  }

  JniHashSet jni_hash_set_;
  jclass jklass_;
  ArtMethod* method_;
  bool loose_check_;
};

class JniHashSetBootImageTest : public CommonRuntimeTest {
 protected:
  void SetUpRuntimeOptions(RuntimeOptions* options) override {
    std::string runtime_args_image;
    runtime_args_image = android::base::StringPrintf("-Ximage:%s", GetCoreArtLocation().c_str());
    options->push_back(std::make_pair(runtime_args_image, nullptr));
  }
};

TEST_F(JniHashSetTest, ReturnType) {
  SetUpForTest("fooI", "(I)I");
  CompareMethod("fooI_V", "(I)V");
  CompareMethod("fooI_B", "(I)B");
  CompareMethod("fooI_C", "(I)C");
  CompareMethod("fooI_S", "(I)S");
  CompareMethod("fooI_Z", "(I)Z");
  CompareMethod("fooI_J", "(I)J");
  CompareMethod("fooI_F", "(I)F");
  CompareMethod("fooI_D", "(I)D");
  CompareMethod("fooI_L", "(I)Ljava/lang/Object;");
}

TEST_F(JniHashSetTest, ArgType) {
  SetUpForTest("fooSI", "(I)I");
  CompareMethod("fooSB", "(B)I");
  CompareMethod("fooSC", "(C)I");
  CompareMethod("fooSS", "(S)I");
  CompareMethod("fooSZ", "(Z)I");
  CompareMethod("fooSL", "(Ljava/lang/Object;)I");
}

TEST_F(JniHashSetTest, FloatingPointArg) {
  SetUpForTest("fooSI", "(I)I");
  CompareMethod("fooS7FI", "(FFFFFFFI)I");
  CompareMethod("fooS3F5DI", "(FFFDDDDDI)I");
  CompareMethod("fooS3F6DI", "(FFFDDDDDDI)I");
}

TEST_F(JniHashSetTest, IntergralArg) {
  SetUpForTest("fooL", "(Ljava/lang/Object;)I");
  CompareMethod("fooL4I", "(Ljava/lang/Object;IIII)I");
  CompareMethod("fooL5I", "(Ljava/lang/Object;IIIII)I");
  CompareMethod("fooL3IJC", "(Ljava/lang/Object;IIIJC)I");
  CompareMethod("fooL3IJCS", "(Ljava/lang/Object;IIIJCS)I");
}

TEST_F(JniHashSetTest, StackOffsetMatters) {
  SetUpForTest("foo7FDF", "(FFFFFFFDF)I");
  CompareMethod("foo9F", "(FFFFFFFFF)I");
  CompareMethod("foo7FIFF", "(FFFFFFFIFF)I");
  SetUpForTest("foo5IJI", "(IIIIIJI)I");
  CompareMethod("foo7I", "(IIIIIII)I");
  CompareMethod("foo5IFII", "(IIIIIFII)I");
  SetUpForTest("fooFDL", "(FDLjava/lang/Object;)I");
  CompareMethod("foo2FL", "(FFLjava/lang/Object;)I");
  CompareMethod("foo3FL", "(FFFLjava/lang/Object;)I");
  CompareMethod("foo2FIL", "(FFILjava/lang/Object;)I");
}

TEST_F(JniHashSetTest, IntLikeRegsMatters) {
  SetUpForTest("fooICFL", "(ICFLjava/lang/Object;)I");
  CompareMethod("foo2IFL", "(IIFLjava/lang/Object;)I");
  CompareMethod("fooICIL", "(ICILjava/lang/Object;)I");
}

TEST_F(JniHashSetTest, FastNative) {
  SetUpForTest("fooI_Fast", "(I)I");
  CompareMethod("fooI_Z_Fast", "(I)Z");
  CompareMethod("fooI_J_Fast", "(I)J");
  SetUpForTest("fooICFL_Fast", "(ICFLjava/lang/Object;)I");
  CompareMethod("foo2IFL_Fast", "(IIFLjava/lang/Object;)I");
  CompareMethod("fooICIL_Fast", "(ICILjava/lang/Object;)I");
  SetUpForTest("fooFDL_Fast", "(FDLjava/lang/Object;)I");
  CompareMethod("foo2FL_Fast", "(FFLjava/lang/Object;)I");
  CompareMethod("foo3FL_Fast", "(FFFLjava/lang/Object;)I");
  CompareMethod("foo2FIL_Fast", "(FFILjava/lang/Object;)I");
  SetUpForTest("foo7F_Fast", "(FFFFFFF)I");
  CompareMethod("foo3F5D_Fast", "(FFFDDDDD)I");
  CompareMethod("foo3F6D_Fast", "(FFFDDDDDD)I");
  SetUpForTest("fooL5I_Fast", "(Ljava/lang/Object;IIIII)I");
  CompareMethod("fooL3IJC_Fast", "(Ljava/lang/Object;IIIJC)I");
  CompareMethod("fooL3IJCS_Fast", "(Ljava/lang/Object;IIIJCS)I");
}

TEST_F(JniHashSetTest, CriticalNative) {
  if (kRuntimeISA == InstructionSet::kX86_64) {
    // In x86_64, the return type seems be ignored in critical function.
    SetLooseCheck(true);
  }
  SetUpForTest("returnInt_Critical", "()I");
  CompareMethod("returnDouble_Critical", "()D");
  CompareMethod("returnLong_Critical", "()J");
  SetUpForTest("foo7F_Critical", "(FFFFFFF)I");
  CompareMethod("foo3F5D_Critical", "(FFFDDDDD)I");
  CompareMethod("foo3F6D_Critical", "(FFFDDDDDD)I");
}

TEST_F(JniHashSetBootImageTest, BootImageSelfCheck) {
  std::vector<gc::space::ImageSpace*> image_spaces =
      Runtime::Current()->GetHeap()->GetBootImageSpaces();
  ASSERT_TRUE(!image_spaces.empty());
  for (gc::space::ImageSpace* space : image_spaces) {
    const ImageHeader& header = space->GetImageHeader();
    PointerSize ptr_size = class_linker_->GetImagePointerSize();
    header.VisitPackedArtMethods([&](ArtMethod& method)
        REQUIRES_SHARED(Locks::mutator_lock_) {
      if (method.IsNative() && !method.IsIntrinsic()) {
        ArtMethod* cmp_method = class_linker_->FindBootNativeMethod(JniHashedKey{&method});
        if (cmp_method != nullptr) {
          uint32_t code_size = method.GetOatMethodQuickCodeSize(ptr_size);
          uint32_t cmp_code_size = cmp_method->GetOatMethodQuickCodeSize(ptr_size);
          const void* quick_code = method.GetOatMethodQuickCode(ptr_size);
          const void* cmp_quick_code = cmp_method->GetOatMethodQuickCode(ptr_size);
          ASSERT_EQ(code_size, cmp_code_size)
              << "method: " << method.PrettyMethod() << ", compared method: "
              << cmp_method->PrettyMethod();
          ASSERT_EQ(memcmp(quick_code, cmp_quick_code, code_size), 0)
              << "method: " << method.PrettyMethod() << ", compared method: "
              << cmp_method->PrettyMethod();
        }
      }
    }, space->Begin(), ptr_size);
  }
}

} // namespace art
