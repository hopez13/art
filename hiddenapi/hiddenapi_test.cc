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

#include <fstream>


#include "base/unix_file/fd_file.h"
#include "common_runtime_test.h"
#include "dex_file-inl.h"
#include "dex_file_loader.h"
#include "exec_utils.h"
#include "zip_archive.h"

namespace art {

class HiddenApiTest : public CommonRuntimeTest {
 protected:
  std::string GetHiddenApiCmd() {
    std::string file_path = GetTestAndroidRoot();
    file_path += "/bin/hiddenapi";
    if (kIsDebugBuild) {
      file_path += "d";
    }
    if (!OS::FileExists(file_path.c_str())) {
      LOG(FATAL) << "Could not find binary " << file_path;
      UNREACHABLE();
    }
    return file_path;
  }

  std::unique_ptr<const DexFile> RunHiddenApi(const ScratchFile& greylist,
                    const ScratchFile& blacklist,
                    const std::vector<std::string>& extra_args,
                    ScratchFile* out_dex) {
    std::string error;
    ZipArchive *jar = ZipArchive::Open(GetTestDexFileName("HiddenApi").c_str(), &error);
    if (jar == nullptr) {
      LOG(FATAL) << "Could not open test file " << GetTestDexFileName("HiddenApi") << ": " << error;
      UNREACHABLE();
    }
    ZipEntry *jar_classes_dex = jar->Find("classes.dex", &error);
    if (jar_classes_dex == nullptr) {
      LOG(FATAL) << "Could not find classes.dex in test file " << GetTestDexFileName("HiddenApi")
                 << ": " << error;
      UNREACHABLE();
    } else if (!jar_classes_dex->ExtractToFile(*out_dex->GetFile(), &error)) {
      LOG(FATAL) << "Could not extract classes.dex from test file "
                 << GetTestDexFileName("HiddenApi") << ": " << error;
      UNREACHABLE();
    }

    std::vector<std::string> argv_str;
    argv_str.push_back(GetHiddenApiCmd());
    argv_str.insert(argv_str.end(), extra_args.begin(), extra_args.end());
    argv_str.push_back("--dex=" + out_dex->GetFilename());
    argv_str.push_back("--greylist=" + greylist.GetFilename());
    argv_str.push_back("--blacklist=" + blacklist.GetFilename());
    int return_code = ExecAndReturnCode(argv_str, &error);
    if (return_code != 0) {
      LOG(FATAL) << "HiddenApi binary exited with unexpected return code " << return_code;
    }
    return OpenDex(*out_dex);
  }

  std::unique_ptr<const DexFile> OpenDex(const ScratchFile &file) {
    std::string error_msg;

    File fd(file.GetFilename(), O_RDONLY, /* check_usage */ false);
    if (fd.Fd() == -1) {
      LOG(FATAL) << "Unable to open file '" << file.GetFilename() << "': " << strerror(errno);
      UNREACHABLE();
    }

    std::unique_ptr<const DexFile> dex_file(DexFileLoader::OpenDex(
        fd.Release(), /* location */ file.GetFilename(), /* verify */ false,
        /* verify_checksum */ true, /* mmap_shared */ false, &error_msg));
    if (dex_file.get() == nullptr) {
      LOG(FATAL) << "Open failed for '" << file.GetFilename() << "' " << error_msg;
      UNREACHABLE();
    } else if (!dex_file->IsStandardDexFile()) {
      LOG(FATAL) << "Expected a standard dex file '" << file.GetFilename() << "'";
      UNREACHABLE();
    }

    return dex_file;
  }

  std::ofstream OpenStream(const ScratchFile &file) {
    std::ofstream ofs(file.GetFilename(), std::ofstream::out);
    if (ofs.fail()) {
      LOG(FATAL) << "Open failed for '" << file.GetFilename() << "' " << strerror(errno);
      UNREACHABLE();
    }
    return ofs;
  }

  const DexFile::ClassDef& FindClass(const char *desc, const DexFile &dex_file) {
    for (uint32_t i = 0; i < dex_file.NumClassDefs(); ++i) {
      const DexFile::ClassDef &class_def = dex_file.GetClassDef(i);
      if (strcmp(desc, dex_file.GetClassDescriptor(class_def)) == 0) {
        return class_def;
      }
    }
    LOG(FATAL) << "Could not find class " << desc;
    UNREACHABLE();
  }

  DexMemberHiddenLevel GetFieldHiddenLevel(const char *name,
                                                 uint32_t expected_visibility,
                                                 const DexFile::ClassDef &class_def,
                                                 const DexFile &dex_file) {
    const uint8_t *class_data = dex_file.GetClassData(class_def);
    if (class_data == nullptr) {
      LOG(FATAL) << "Class " << dex_file.GetClassDescriptor(class_def) << " has no data";
      UNREACHABLE();
    }

    for (ClassDataItemIterator it(dex_file, class_data); it.HasNext(); it.Next()) {
      if (it.IsAtMethod()) {
        break;
      }
      const DexFile::FieldId &fid = dex_file.GetFieldId(it.GetMemberIndex());
      if (strcmp(name, dex_file.GetFieldName(fid)) == 0) {
        uint32_t actual_visibility = it.GetFieldAccessFlags() & kAccVisibilityFlags;
        if (actual_visibility != expected_visibility) {
          LOG(FATAL) << "Field " << name << " in class " << dex_file.GetClassDescriptor(class_def)
                     << " does not have the expected visibility flags (" << expected_visibility
                     << " != " << actual_visibility << ")";
          UNREACHABLE();
        }
        return it.GetMemberHiddenLevel();
      }
    }

    LOG(FATAL) << "Could not find field " << name << " in class "
               << dex_file.GetClassDescriptor(class_def);
    UNREACHABLE();
  }

  DexMemberHiddenLevel GetMethodHiddenLevel(const char *name,
                                                  uint32_t expected_visibility,
                                                  bool expected_native,
                                                  const DexFile::ClassDef &class_def,
                                                  const DexFile &dex_file) {
    const uint8_t *class_data = dex_file.GetClassData(class_def);
    if (class_data == nullptr) {
      LOG(FATAL) << "Class " << dex_file.GetClassDescriptor(class_def) << " has no data";
      UNREACHABLE();
    }

    for (ClassDataItemIterator it(dex_file, class_data); it.HasNext(); it.Next()) {
      if (!it.IsAtMethod()) {
        continue;
      }
      const DexFile::MethodId &mid = dex_file.GetMethodId(it.GetMemberIndex());
      if (strcmp(name, dex_file.GetMethodName(mid)) == 0) {
        if (expected_native != it.MemberIsNative()) {
          LOG(FATAL) << "Expected native=" << expected_native << " for method " << name
                     << " in class " << dex_file.GetClassDescriptor(class_def);
          UNREACHABLE();
        }
        uint32_t actual_visibility = it.GetMethodAccessFlags() & kAccVisibilityFlags;
        if (actual_visibility != expected_visibility) {
          LOG(FATAL) << "Method " << name << " in class " << dex_file.GetClassDescriptor(class_def)
                     << " does not have the expected visibility flags (" << expected_visibility
                     << " != " << actual_visibility << ")";
          UNREACHABLE();
        }
        return it.GetMemberHiddenLevel();
      }
    }

    LOG(FATAL) << "Could not find method " << name << " in class "
               << dex_file.GetClassDescriptor(class_def);
    UNREACHABLE();
  }

  DexMemberHiddenLevel GetIFieldHiddenLevel(const DexFile &dex_file) {
    return GetFieldHiddenLevel("ifield", kAccPublic, FindClass("LMain;", dex_file), dex_file);
  }

  DexMemberHiddenLevel GetSFieldHiddenLevel(const DexFile &dex_file) {
    return GetFieldHiddenLevel("sfield", kAccPrivate, FindClass("LMain;", dex_file), dex_file);
  }

  DexMemberHiddenLevel GetIMethodHiddenLevel(const DexFile &dex_file) {
    return GetMethodHiddenLevel(
        "imethod", 0, /* native */ false, FindClass("LMain;", dex_file), dex_file);
  }

  DexMemberHiddenLevel GetSMethodHiddenLevel(const DexFile &dex_file) {
    return GetMethodHiddenLevel(
        "smethod", kAccPublic, /* native */ false, FindClass("LMain;", dex_file), dex_file);
  }

  DexMemberHiddenLevel GetINMethodHiddenLevel(const DexFile &dex_file) {
    return GetMethodHiddenLevel(
        "inmethod", kAccPublic, /* native */ true, FindClass("LMain;", dex_file), dex_file);
  }

  DexMemberHiddenLevel GetSNMethodHiddenLevel(const DexFile &dex_file) {
    return GetMethodHiddenLevel(
        "snmethod", kAccProtected, /* native */ true, FindClass("LMain;", dex_file), dex_file);
  }
};

TEST_F(HiddenApiTest, InstanceFieldNoMatch) {
  ScratchFile dex, greylist, blacklist;
  OpenStream(greylist) << "LMain;->ifield:LBadType1;" << std::endl;
  OpenStream(blacklist) << "LMain;->ifield:LBadType2;" << std::endl;
  auto dex_file = RunHiddenApi(greylist, blacklist, {}, &dex);
  ASSERT_EQ(kDexHiddenWhitelist, GetIFieldHiddenLevel(*dex_file));
}

TEST_F(HiddenApiTest, InstanceFieldGreylistMatch) {
  ScratchFile dex, greylist, blacklist;
  OpenStream(greylist) << "LMain;->ifield:I" << std::endl;
  OpenStream(blacklist) << "LMain;->ifield:LBadType2;" << std::endl;
  auto dex_file = RunHiddenApi(greylist, blacklist, {}, &dex);
  ASSERT_EQ(kDexHiddenGreylist, GetIFieldHiddenLevel(*dex_file));
}

TEST_F(HiddenApiTest, InstanceFieldBlacklistMatch) {
  ScratchFile dex, greylist, blacklist;
  OpenStream(greylist) << "LMain;->ifield:LBadType1;" << std::endl;
  OpenStream(blacklist) << "LMain;->ifield:I" << std::endl;
  auto dex_file = RunHiddenApi(greylist, blacklist, {}, &dex);
  ASSERT_EQ(kDexHiddenBlacklist, GetIFieldHiddenLevel(*dex_file));
}

TEST_F(HiddenApiTest, InstanceFieldBothListsMatch) {
  ScratchFile dex, greylist, blacklist;
  OpenStream(greylist) << "LMain;->ifield:I" << std::endl;
  OpenStream(blacklist) << "LMain;->ifield:I" << std::endl;
  auto dex_file = RunHiddenApi(greylist, blacklist, {}, &dex);
  ASSERT_EQ(kDexHiddenBlacklist, GetIFieldHiddenLevel(*dex_file));
}

TEST_F(HiddenApiTest, StaticFieldNoMatch) {
  ScratchFile dex, greylist, blacklist;
  OpenStream(greylist) << "LMain;->sfield:LBadType1;" << std::endl;
  OpenStream(blacklist) << "LMain;->sfield:LBadType2;" << std::endl;
  auto dex_file = RunHiddenApi(greylist, blacklist, {}, &dex);
  ASSERT_EQ(kDexHiddenWhitelist, GetSFieldHiddenLevel(*dex_file));
}

TEST_F(HiddenApiTest, StaticFieldGreylistMatch) {
  ScratchFile dex, greylist, blacklist;
  OpenStream(greylist) << "LMain;->sfield:Ljava/lang/Object;" << std::endl;
  OpenStream(blacklist) << "LMain;->sfield:LBadType2;" << std::endl;
  auto dex_file = RunHiddenApi(greylist, blacklist, {}, &dex);
  ASSERT_EQ(kDexHiddenGreylist, GetSFieldHiddenLevel(*dex_file));
}

TEST_F(HiddenApiTest, StaticFieldBlacklistMatch) {
  ScratchFile dex, greylist, blacklist;
  OpenStream(greylist) << "LMain;->sfield:LBadType1;" << std::endl;
  OpenStream(blacklist) << "LMain;->sfield:Ljava/lang/Object;" << std::endl;
  auto dex_file = RunHiddenApi(greylist, blacklist, {}, &dex);
  ASSERT_EQ(kDexHiddenBlacklist, GetSFieldHiddenLevel(*dex_file));
}

TEST_F(HiddenApiTest, StaticFieldBothListsMatch) {
  ScratchFile dex, greylist, blacklist;
  OpenStream(greylist) << "LMain;->sfield:Ljava/lang/Object;" << std::endl;
  OpenStream(blacklist) << "LMain;->sfield:Ljava/lang/Object;" << std::endl;
  auto dex_file = RunHiddenApi(greylist, blacklist, {}, &dex);
  ASSERT_EQ(kDexHiddenBlacklist, GetSFieldHiddenLevel(*dex_file));
}

TEST_F(HiddenApiTest, InstanceMethodNoMatch) {
  ScratchFile dex, greylist, blacklist;
  OpenStream(greylist) << "LMain;->imethod(LBadType1;)V" << std::endl;
  OpenStream(blacklist) << "LMain;->imethod(LBadType2;)V" << std::endl;
  auto dex_file = RunHiddenApi(greylist, blacklist, {}, &dex);
  ASSERT_EQ(kDexHiddenWhitelist, GetIMethodHiddenLevel(*dex_file));
}

TEST_F(HiddenApiTest, InstanceMethodGreylistMatch) {
  ScratchFile dex, greylist, blacklist;
  OpenStream(greylist) << "LMain;->imethod(J)V" << std::endl;
  OpenStream(blacklist) << "LMain;->imethod(LBadType2;)V" << std::endl;
  auto dex_file = RunHiddenApi(greylist, blacklist, {}, &dex);
  ASSERT_EQ(kDexHiddenGreylist, GetIMethodHiddenLevel(*dex_file));
}

TEST_F(HiddenApiTest, InstanceMethodBlacklistMatch) {
  ScratchFile dex, greylist, blacklist;
  OpenStream(greylist) << "LMain;->imethod(LBadType1;)V" << std::endl;
  OpenStream(blacklist) << "LMain;->imethod(J)V" << std::endl;
  auto dex_file = RunHiddenApi(greylist, blacklist, {}, &dex);
  ASSERT_EQ(kDexHiddenBlacklist, GetIMethodHiddenLevel(*dex_file));
}

TEST_F(HiddenApiTest, InstanceMethodBothListsMatch) {
  ScratchFile dex, greylist, blacklist;
  OpenStream(greylist) << "LMain;->imethod(J)V" << std::endl;
  OpenStream(blacklist) << "LMain;->imethod(J)V" << std::endl;
  auto dex_file = RunHiddenApi(greylist, blacklist, {}, &dex);
  ASSERT_EQ(kDexHiddenBlacklist, GetIMethodHiddenLevel(*dex_file));
}

TEST_F(HiddenApiTest, StaticMethodNoMatch) {
  ScratchFile dex, greylist, blacklist;
  OpenStream(greylist) << "LMain;->smethod(LBadType1;)V" << std::endl;
  OpenStream(blacklist) << "LMain;->smethod(LBadType2;)V" << std::endl;
  auto dex_file = RunHiddenApi(greylist, blacklist, {}, &dex);
  ASSERT_EQ(kDexHiddenWhitelist, GetSMethodHiddenLevel(*dex_file));
}

TEST_F(HiddenApiTest, StaticMethodGreylistMatch) {
  ScratchFile dex, greylist, blacklist;
  OpenStream(greylist) << "LMain;->smethod(Ljava/lang/Object;)V" << std::endl;
  OpenStream(blacklist) << "LMain;->smethod(LBadType2;)V" << std::endl;
  auto dex_file = RunHiddenApi(greylist, blacklist, {}, &dex);
  ASSERT_EQ(kDexHiddenGreylist, GetSMethodHiddenLevel(*dex_file));
}

TEST_F(HiddenApiTest, StaticMethodBlacklistMatch) {
  ScratchFile dex, greylist, blacklist;
  OpenStream(greylist) << "LMain;->smethod(LBadType1;)V" << std::endl;
  OpenStream(blacklist) << "LMain;->smethod(Ljava/lang/Object;)V" << std::endl;
  auto dex_file = RunHiddenApi(greylist, blacklist, {}, &dex);
  ASSERT_EQ(kDexHiddenBlacklist, GetSMethodHiddenLevel(*dex_file));
}

TEST_F(HiddenApiTest, StaticMethodBothListsMatch) {
  ScratchFile dex, greylist, blacklist;
  OpenStream(greylist) << "LMain;->smethod(Ljava/lang/Object;)V" << std::endl;
  OpenStream(blacklist) << "LMain;->smethod(Ljava/lang/Object;)V" << std::endl;
  auto dex_file = RunHiddenApi(greylist, blacklist, {}, &dex);
  ASSERT_EQ(kDexHiddenBlacklist, GetSMethodHiddenLevel(*dex_file));
}

TEST_F(HiddenApiTest, InstanceNativeMethodNoMatch) {
  ScratchFile dex, greylist, blacklist;
  OpenStream(greylist) << "LMain;->inmethod(LBadType1;)V" << std::endl;
  OpenStream(blacklist) << "LMain;->inmethod(LBadType2;)V" << std::endl;
  auto dex_file = RunHiddenApi(greylist, blacklist, {}, &dex);
  ASSERT_EQ(kDexHiddenWhitelist, GetINMethodHiddenLevel(*dex_file));
}

TEST_F(HiddenApiTest, InstanceNativeMethodGreylistMatch) {
  ScratchFile dex, greylist, blacklist;
  OpenStream(greylist) << "LMain;->inmethod(C)V" << std::endl;
  OpenStream(blacklist) << "LMain;->inmethod(LBadType2;)V" << std::endl;
  auto dex_file = RunHiddenApi(greylist, blacklist, {}, &dex);
  ASSERT_EQ(kDexHiddenGreylist, GetINMethodHiddenLevel(*dex_file));
}

TEST_F(HiddenApiTest, InstanceNativeMethodBlacklistMatch) {
  ScratchFile dex, greylist, blacklist;
  OpenStream(greylist) << "LMain;->inmethod(LBadType1;)V" << std::endl;
  OpenStream(blacklist) << "LMain;->inmethod(C)V" << std::endl;
  auto dex_file = RunHiddenApi(greylist, blacklist, {}, &dex);
  ASSERT_EQ(kDexHiddenBlacklist, GetINMethodHiddenLevel(*dex_file));
}

TEST_F(HiddenApiTest, InstanceNativeMethodBothListsMatch) {
  ScratchFile dex, greylist, blacklist;
  OpenStream(greylist) << "LMain;->inmethod(C)V" << std::endl;
  OpenStream(blacklist) << "LMain;->inmethod(C)V" << std::endl;
  auto dex_file = RunHiddenApi(greylist, blacklist, {}, &dex);
  ASSERT_EQ(kDexHiddenBlacklist, GetINMethodHiddenLevel(*dex_file));
}

TEST_F(HiddenApiTest, StaticNativeMethodNoMatch) {
  ScratchFile dex, greylist, blacklist;
  OpenStream(greylist) << "LMain;->snmethod(LBadType1;)V" << std::endl;
  OpenStream(blacklist) << "LMain;->snmethod(LBadType2;)V" << std::endl;
  auto dex_file = RunHiddenApi(greylist, blacklist, {}, &dex);
  ASSERT_EQ(kDexHiddenWhitelist, GetSNMethodHiddenLevel(*dex_file));
}

TEST_F(HiddenApiTest, StaticNativeMethodGreylistMatch) {
  ScratchFile dex, greylist, blacklist;
  OpenStream(greylist) << "LMain;->snmethod(Ljava/lang/Integer;)V" << std::endl;
  OpenStream(blacklist) << "LMain;->snmethod(LBadType2;)V" << std::endl;
  auto dex_file = RunHiddenApi(greylist, blacklist, {}, &dex);
  ASSERT_EQ(kDexHiddenGreylist, GetSNMethodHiddenLevel(*dex_file));
}

TEST_F(HiddenApiTest, StaticNativeMethodBlacklistMatch) {
  ScratchFile dex, greylist, blacklist;
  OpenStream(greylist) << "LMain;->snmethod(LBadType1;)V" << std::endl;
  OpenStream(blacklist) << "LMain;->snmethod(Ljava/lang/Integer;)V" << std::endl;
  auto dex_file = RunHiddenApi(greylist, blacklist, {}, &dex);
  ASSERT_EQ(kDexHiddenBlacklist, GetSNMethodHiddenLevel(*dex_file));
}

TEST_F(HiddenApiTest, StaticNativeMethodBothListsMatch) {
  ScratchFile dex, greylist, blacklist;
  OpenStream(greylist) << "LMain;->snmethod(Ljava/lang/Integer;)V" << std::endl;
  OpenStream(blacklist) << "LMain;->snmethod(Ljava/lang/Integer;)V" << std::endl;
  auto dex_file = RunHiddenApi(greylist, blacklist, {}, &dex);
  ASSERT_EQ(kDexHiddenBlacklist, GetSNMethodHiddenLevel(*dex_file));
}

}  // namespace art
