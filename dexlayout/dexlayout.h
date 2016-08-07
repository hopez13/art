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
 *
 * Header file of the dexlayout utility.
 *
 * This is a tool to read dex files into an internal representation,
 * reorganize the representation, and emit dex files with a better
 * file layout.
 */

#ifndef ART_DEXLAYOUT_DEXLAYOUT_H_
#define ART_DEXLAYOUT_DEXLAYOUT_H_

#include <dex_ir.h>

#include <stdint.h>
#include <stdio.h>

namespace art {

/* Supported output formats. */
enum OutputFormat {
  OUTPUT_PLAIN = 0,  // default
  OUTPUT_XML,        // XML-style
};

/* Command-line options. */
struct Options {
  bool buildDexir;
  bool checksumOnly;
  bool disassemble;
  bool exportsOnly;
  bool ignoreBadChecksum;
  bool showAnnotations;
  bool showCfg;
  bool showFileHeaders;
  bool showSectionHeaders;
  bool verbose;
  OutputFormat outputFormat;
  const char* outputFileName;
};

/* Prototypes. */
extern struct Options gOptions;
extern FILE* gOutFile;
int processFile(const char* fileName);

}  // namespace art

#endif  // ART_DEXLAYOUT_DEXLAYOUT_H_
