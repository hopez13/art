/*
 * Copyright (C) 2014 The Android Open Source Project
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

#include "instruction_set_features_x86.h"

#include <fstream>
#include <sstream>

#include "android-base/stringprintf.h"
#include "android-base/strings.h"

#include "arch/x86_64/instruction_set_features_x86_64.h"
#include "base/logging.h"

namespace art {

using android::base::StringPrintf;

// Feature-support arrays.

static constexpr const char* x86_known_variants[] = {
  "haswell",
};

static constexpr const char* x86_variants_with_avx[] = {
  "haswell",
};

static constexpr const char* x86_variants_with_avx2[] = {
  "haswell",
};

X86FeaturesUniquePtr X86InstructionSetFeatures::Create(bool x86_64,
                                                       bool has_AVX,
                                                       bool has_AVX2) {
  if (x86_64) {
    return X86FeaturesUniquePtr(new X86_64InstructionSetFeatures(has_AVX, has_AVX2));
  } else {
    return X86FeaturesUniquePtr(new X86InstructionSetFeatures(has_AVX, has_AVX2));
  }
}

X86FeaturesUniquePtr X86InstructionSetFeatures::FromVariant(
    const std::string& variant,
    std::string* error_msg ATTRIBUTE_UNUSED,
    bool x86_64) {
  bool has_AVX = FindVariantInArray(x86_variants_with_avx,
                                    arraysize(x86_variants_with_avx), variant);
  bool has_AVX2 = FindVariantInArray(x86_variants_with_avx2,
                                     arraysize(x86_variants_with_avx2), variant);

  // Verify that variant is known.
  bool known_variant = FindVariantInArray(x86_known_variants, arraysize(x86_known_variants),
                                          variant);
  if (!known_variant && variant != "default") {
    LOG(WARNING) << "Unexpected CPU variant for X86 using defaults: " << variant;
  }

  return Create(x86_64, has_AVX, has_AVX2);
}

X86FeaturesUniquePtr X86InstructionSetFeatures::FromBitmap(uint32_t bitmap, bool x86_64) {
  bool has_AVX = (bitmap & kAvxBitfield) != 0;
  bool has_AVX2 = (bitmap & kAvxBitfield) != 0;
  return Create(x86_64, has_AVX, has_AVX2);
}

X86FeaturesUniquePtr X86InstructionSetFeatures::FromCppDefines(bool x86_64) {
#ifndef __AVX__
  const bool has_AVX = false;
#else
  const bool has_AVX = true;
#endif
#ifndef __AVX2__
  const bool has_AVX2 = false;
#else
  const bool has_AVX2 = true;
#endif
  return Create(x86_64, has_AVX, has_AVX2);
}

X86FeaturesUniquePtr X86InstructionSetFeatures::FromCpuInfo(bool x86_64) {
  // Look in /proc/cpuinfo for features we need.  Only use this when we can guarantee that
  // the kernel puts the appropriate feature flags in here.  Sometimes it doesn't.
  bool has_AVX = false;
  bool has_AVX2 = false;

  std::ifstream in("/proc/cpuinfo");
  if (!in.fail()) {
    while (!in.eof()) {
      std::string line;
      std::getline(in, line);
      if (!in.eof()) {
        LOG(INFO) << "cpuinfo line: " << line;
        if (line.find("flags") != std::string::npos) {
          LOG(INFO) << "found flags";
          if (line.find("avx") != std::string::npos) {
            has_AVX = true;
          }
          if (line.find("avx2") != std::string::npos) {
            has_AVX2 = true;
          }
        }
      }
    }
    in.close();
  } else {
    LOG(ERROR) << "Failed to open /proc/cpuinfo";
  }
  return Create(x86_64, has_AVX, has_AVX2);
}

X86FeaturesUniquePtr X86InstructionSetFeatures::FromHwcap(bool x86_64) {
  UNIMPLEMENTED(WARNING);
  return FromCppDefines(x86_64);
}

X86FeaturesUniquePtr X86InstructionSetFeatures::FromAssembly(bool x86_64) {
  UNIMPLEMENTED(WARNING);
  return FromCppDefines(x86_64);
}

bool X86InstructionSetFeatures::Equals(const InstructionSetFeatures* other) const {
  if (GetInstructionSet() != other->GetInstructionSet()) {
    return false;
  }
  const X86InstructionSetFeatures* other_as_x86 = other->AsX86InstructionSetFeatures();
  return (has_AVX_ == other_as_x86->has_AVX_) && (has_AVX2_ == other_as_x86->has_AVX2_);
}

uint32_t X86InstructionSetFeatures::AsBitmap() const {
  return (has_AVX_ ? kAvxBitfield : 0) | (has_AVX2_ ? kAvx2Bitfield : 0);
}

std::string X86InstructionSetFeatures::GetFeatureString() const {
  std::string result;
  if (has_AVX_) {
    result += "avx";
  } else {
    result += "-avx";
  }
  if (has_AVX2_) {
    result += ",avx2";
  } else {
    result += ",-avx2";
  }
  return result;
}

std::unique_ptr<const InstructionSetFeatures> X86InstructionSetFeatures::AddFeaturesFromSplitString(
    const std::vector<std::string>& features,
    bool x86_64,
    std::string* error_msg) const {
  bool has_AVX = has_AVX_;
  bool has_AVX2 = has_AVX2_;
  for (auto i = features.begin(); i != features.end(); i++) {
    std::string feature = android::base::Trim(*i);
    if (feature == "avx") {
      has_AVX = true;
    } else if (feature == "-avx") {
      has_AVX = false;
    } else if (feature == "avx2") {
      has_AVX2 = true;
    } else if (feature == "-avx2") {
      has_AVX2 = false;
    } else {
      *error_msg = StringPrintf("Unknown instruction set feature: '%s'", feature.c_str());
      return nullptr;
    }
  }
  return Create(x86_64, has_AVX, has_AVX2);
}

}  // namespace art
