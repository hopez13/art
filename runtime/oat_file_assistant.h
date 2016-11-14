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

#ifndef ART_RUNTIME_OAT_FILE_ASSISTANT_H_
#define ART_RUNTIME_OAT_FILE_ASSISTANT_H_

#include <cstdint>
#include <memory>
#include <sstream>
#include <string>

#include "arch/instruction_set.h"
#include "base/scoped_flock.h"
#include "base/unix_file/fd_file.h"
#include "compiler_filter.h"
#include "oat_file.h"
#include "os.h"

namespace art {

namespace gc {
namespace space {
class ImageSpace;
}  // namespace space
}  // namespace gc

class OatFileAssistant;

enum DexOptNeeded {
  // No dexopt should (or can) be done to update the code for this dex
  // location.
  // Matches Java: dalvik.system.DexFile.NO_DEXOPT_NEEDED = 0
  kNoDexOptNeeded = 0,

  // dex2oat should be run to update the code for this dex location without
  // use of an existing vdex file.
  // Matches Java: dalvik.system.DexFile.DEX2OAT_FROM_SCRATCH = 1
  kDex2OatFromScratch = 1,

  // dex2oat should be run to update the apk/jar using the vdex file as
  // input. The vdex file is up to date with respect to the apk/jar, but is
  // out of date with respect to the boot image.
  // Matches Java: dalvik.system.DexFile.DEX2OAT_FOR_BOOT_IMAGE
  kDex2OatForBootImage = 2,

  // dex2oat should be run to update the apk/jar using the vdex file input.
  // The vdex file is up to date with respect to the apk/jar and boot image.
  // The existing oat file is out of date with respect to the compiler
  // filter.
  // Matches Java: dalvik.system.DexFile.DEX2OAT_FOR_FILTER
  kDex2OatForFilter = 3,

  // dex2oat should be run to update the apk/jar using the vdex file as
  // input. The vdex file is up to date with respect to the apk/jar and boot
  // image. patchoat cannot be called because the existing oat file does not
  // have the necessary patch information.
  // Matches Java: dalvik.system.DexFile.DEX2OAT_FOR_RELOCATION
  kDex2OatForRelocation = 4,

  // patchoat should be run to update the apk/jar.
  // Matches Java: dalvik.system.DexFile.PATCHOAT_FOR_RELOCATION
  kPatchoatForRelocation = 5,
};

enum OatStatus {
  // kOatOutOfDate - The oat file does not exist, is unreadable, corrupt, or
  // is out of date with respect to the dex file.
  kOatOutOfDate = 0,

  // kOatBootImageOutOfDate - The oat file is up to date with respect to the
  // dex file, but is out of date with respect to the boot image.
  kOatBootImageOutOfDate = 1,

  // kOatRelocationOutOfDate - The oat file is up to date with respect to
  // the dex file and boot image, but contains compiled code that has the
  // wrong patch delta with respect to the boot image. Patchoat should be
  // run on the oat file to update the patch delta of the compiled code to
  // match the boot image.
  kOatRelocationOutOfDate = 2,

  // kOatUpToDate - The oat file is completely up to date with respect to
  // the dex file and boot image.
  kOatUpToDate = 3,
};

std::ostream& operator << (std::ostream& stream, const OatStatus status);

// Class used in conjunction with an OatFileAssistant for getting information
// about the status of a specific oat file on disk.
class OatFileInfo {
 public:
  // Initially the info is for no file in particular. It will treat the
  // file as out of date until Reset is called with a real filename to use
  // the cache for.
  // Pass true for is_oat_location if the information associated with this
  // OatFileInfo is for the oat location, as opposed to the odex location.
  OatFileInfo(OatFileAssistant* oat_file_assistant, bool is_oat_location);

  // Returns the value of the is_oat_location field passed at time of
  // construction.
  bool IsOatLocation();

  // Returns the name of the oat file on disk, or null if no filename has yet
  // been provided.
  const std::string* Filename();

  // Returns true if the oat file exists.
  bool Exists();

  // Returns true if the oat file is unuseable.
  bool IsOutOfDate();

  // Returns the status of the oat file.
  OatStatus Status();

  // Returns the value of the compiler filter for the oat file.
  // Must only be called if the associated file Exists().
  CompilerFilter::Filter CompilerFilter();

  // Return the DexOptNeeded value for this oat file with respect to the
  // given target_compilation_filter.
  // profile_changed should be true to indicate the profile has recently
  // changed for this dex location.
  DexOptNeeded GetDexOptNeeded(CompilerFilter::Filter target_compiler_filter,
                               bool profile_changed);

  // Returns a reference to the loaded file.
  // Loads the file if needed. Returns null if the file failed to load.
  // Ownership of the OatFile belongs to the OatFileInfo object; the caller
  // shouldn't clean up or free the returned pointer.
  const OatFile* GetFile();

  // Returns true if the oat file is currently opened executable.
  bool IsExecutable();

  // Returns true if the oat file has patch info required to run patchoat.
  bool HasPatchInfo();

  // Clear any cached information about the oat file that depends on the
  // contents of the file. This does not reset the provided filename.
  void Reset();

  // Clear any cached information and switch to getting info about the oat
  // file with the given filename.
  //
  // load_executable should be true if the caller intends to try and load
  // executable code for this dex location.
  void Reset(const std::string& filename, bool load_executable);

  // Release the loaded oat file for runtime use.
  // Returns null if the oat file hasn't been loaded or is out of date.
  // Ensures the returned file is not loaded executable if it has unuseable
  // compiled code.
  //
  // After this call, no other methods of the OatFileInfo should be
  // called, because access to the loaded oat file has been taken away from
  // the OatFileInfo object.
  std::unique_ptr<OatFile> ReleaseFileForUse();

 private:
  // Returns true if the compiler filter used to generate the file is at
  // least as good as the given target filter. profile_changed should be
  // true to indicate the profile has recently changed for this dex
  // location.
  bool CompilerFilterIsOkay(CompilerFilter::Filter target, bool profile_changed);

  // Release the loaded oat file.
  // Returns null if the oat file hasn't been loaded.
  //
  // After this call, no other methods of the OatFileInfo should be
  // called, because access to the loaded oat file has been taken away from
  // the OatFileInfo object.
  std::unique_ptr<OatFile> ReleaseFile();

  OatFileAssistant* oat_file_assistant_;
  bool is_oat_location_;

  // Whether we will attempt to load oat files executable.
  bool load_executable_ = false;

  bool filename_provided_ = false;
  std::string filename_;

  bool load_attempted_ = false;
  std::unique_ptr<OatFile> file_;

  bool status_attempted_ = false;
  OatStatus status_;

  // For debugging only.
  // If this flag is set, the file has been released to the user and the
  // OatFileInfo object is in a bad state and should no longer be used.
  bool file_released_ = false;
};

// Class for assisting with oat file management.
//
// This class collects common utilities for determining the status of an oat
// file on the device, updating the oat file, and loading the oat file.
//
// The oat file assistant is intended to be used with dex locations not on the
// boot class path. See the IsInBootClassPath method for a way to check if the
// dex location is in the boot class path.
class OatFileAssistant {
 public:
  // Constructs an OatFileAssistant object to assist the oat file
  // corresponding to the given dex location with the target instruction set.
  //
  // The dex_location must not be null and should remain available and
  // unchanged for the duration of the lifetime of the OatFileAssistant object.
  // Typically the dex_location is the absolute path to the original,
  // un-optimized dex file.
  //
  // Note: Currently the dex_location must have an extension.
  // TODO: Relax this restriction?
  //
  // The isa should be either the 32 bit or 64 bit variant for the current
  // device. For example, on an arm device, use arm or arm64. An oat file can
  // be loaded executable only if the ISA matches the current runtime.
  //
  // load_executable should be true if the caller intends to try and load
  // executable code for this dex location.
  OatFileAssistant(const char* dex_location,
                   const InstructionSet isa,
                   bool load_executable);

  // Constructs an OatFileAssistant, providing an explicit target oat_location
  // to use instead of the standard oat location.
  OatFileAssistant(const char* dex_location,
                   const char* oat_location,
                   const InstructionSet isa,
                   bool load_executable);

  ~OatFileAssistant();

  // Returns the dex location this OatFileAssistant object is assisting with.
  std::string DexLocation() const;

  // Returns true if the dex location refers to an element of the boot class
  // path.
  bool IsInBootClassPath();

  // Obtains a lock on the target oat file.
  // Only one OatFileAssistant object can hold the lock for a target oat file
  // at a time. The Lock is released automatically when the OatFileAssistant
  // object goes out of scope. The Lock() method must not be called if the
  // lock has already been acquired.
  //
  // Returns true on success.
  // Returns false on error, in which case error_msg will contain more
  // information on the error.
  //
  // The 'error_msg' argument must not be null.
  //
  // This is intended to be used to avoid race conditions when multiple
  // processes generate oat files, such as when a foreground Activity and
  // a background Service both use DexClassLoaders pointing to the same dex
  // file.
  bool Lock(std::string* error_msg);

  // Return what action needs to be taken to produce up-to-date code for this
  // dex location that is at least as good as an oat file generated with the
  // given compiler filter. profile_changed should be true to indicate the
  // profile has recently changed for this dex location.
  // Returns a positive status code if the status refers to the oat file in
  // the oat location. Returns a negative status code if the status refers to
  // the oat file in the odex location.
  int GetDexOptNeeded(CompilerFilter::Filter target_compiler_filter, bool profile_changed = false);

  // Returns true if there is up-to-date code for this dex location,
  // irrespective of the compiler filter of the up-to-date code.
  bool IsUpToDate();

  // Return code used when attempting to generate updated code.
  enum ResultOfAttemptToUpdate {
    kUpdateFailed,        // We tried making the code up to date, but
                          // encountered an unexpected failure.
    kUpdateNotAttempted,  // We wanted to update the code, but determined we
                          // should not make the attempt.
    kUpdateSucceeded      // We successfully made the code up to date
                          // (possibly by doing nothing).
  };

  // Attempts to generate or relocate the oat file as needed to make it up to
  // date based on the current runtime and compiler options.
  // profile_changed should be true to indicate the profile has recently
  // changed for this dex location.
  //
  // Returns the result of attempting to update the code.
  //
  // If the result is not kUpdateSucceeded, the value of error_msg will be set
  // to a string describing why there was a failure or the update was not
  // attempted. error_msg must not be null.
  ResultOfAttemptToUpdate MakeUpToDate(bool profile_changed, std::string* error_msg);

  // Returns an oat file that can be used for loading dex files.
  // Returns null if no suitable oat file was found.
  //
  // After this call, no other methods of the OatFileAssistant should be
  // called, because access to the loaded oat file has been taken away from
  // the OatFileAssistant object.
  std::unique_ptr<OatFile> GetBestOatFile();

  // Returns a human readable description of the status of the code for the
  // dex file. The returned description is for debugging purposes only.
  std::string GetStatusDump();

  // Open and returns an image space associated with the oat file.
  static std::unique_ptr<gc::space::ImageSpace> OpenImageSpace(const OatFile* oat_file);

  // Loads the dex files in the given oat file for the given dex location.
  // The oat file should be up to date for the given dex location.
  // This loads multiple dex files in the case of multidex.
  // Returns an empty vector if no dex files for that location could be loaded
  // from the oat file.
  //
  // The caller is responsible for freeing the dex_files returned, if any. The
  // dex_files will only remain valid as long as the oat_file is valid.
  static std::vector<std::unique_ptr<const DexFile>> LoadDexFiles(
      const OatFile& oat_file, const char* dex_location);

  // Returns true if there are dex files in the original dex location that can
  // be compiled with dex2oat for this dex location.
  // Returns false if there is no original dex file, or if the original dex
  // file is an apk/zip without a classes.dex entry.
  bool HasOriginalDexFiles();

  // Returns information about the oat file in the odex location.
  //
  // If the dex file has been installed with a compiled oat file alongside
  // it, the compiled oat file will have the extension .odex, and is referred
  // to as the odex file. It is called odex for legacy reasons; the file is
  // really an oat file. The odex file will often, but not always, have a
  // patch delta of 0 and need to be relocated before use for the purposes of
  // ASLR. The odex file is treated as if it were read-only.
  OatFileInfo& GetOdexInfo();

  // Returns information about the oat file in the oat location.
  //
  // When the dex files is compiled on the target device, the oat file is the
  // result. The oat file will have been relocated to some
  // (possibly-out-of-date) offset for ASLR.
  OatFileInfo& GetOatInfo();

  // Returns information about the best oat file available to use.
  OatFileInfo& GetBestInfo();

  // Returns the status for a given opened oat file with respect to the dex
  // location.
  OatStatus GivenOatFileStatus(const OatFile& file);

  // Generates the oat file by relocation from the named input file.
  // This does not check the current status before attempting to relocate the
  // oat file.
  //
  // If the result is not kUpdateSucceeded, the value of error_msg will be set
  // to a string describing why there was a failure or the update was not
  // attempted. error_msg must not be null.
  ResultOfAttemptToUpdate RelocateOatFile(const std::string* input_file, std::string* error_msg);

  // Generate the oat file from the dex file using the current runtime
  // compiler options.
  // This does not check the current status before attempting to generate the
  // oat file.
  //
  // If the result is not kUpdateSucceeded, the value of error_msg will be set
  // to a string describing why there was a failure or the update was not
  // attempted. error_msg must not be null.
  ResultOfAttemptToUpdate GenerateOatFile(std::string* error_msg);

  // Executes dex2oat using the current runtime configuration overridden with
  // the given arguments. This does not check to see if dex2oat is enabled in
  // the runtime configuration.
  // Returns true on success.
  //
  // If there is a failure, the value of error_msg will be set to a string
  // describing why there was failure. error_msg must not be null.
  //
  // TODO: The OatFileAssistant probably isn't the right place to have this
  // function.
  static bool Dex2Oat(const std::vector<std::string>& args, std::string* error_msg);

  // Constructs the odex file name for the given dex location.
  // Returns true on success, in which case odex_filename is set to the odex
  // file name.
  // Returns false on error, in which case error_msg describes the error and
  // odex_filename is not changed.
  // Neither odex_filename nor error_msg may be null.
  static bool DexLocationToOdexFilename(const std::string& location,
                                        InstructionSet isa,
                                        std::string* odex_filename,
                                        std::string* error_msg);

  // Constructs the oat file name for the given dex location.
  // Returns true on success, in which case oat_filename is set to the oat
  // file name.
  // Returns false on error, in which case error_msg describes the error and
  // oat_filename is not changed.
  // Neither oat_filename nor error_msg may be null.
  static bool DexLocationToOatFilename(const std::string& location,
                                       InstructionSet isa,
                                       std::string* oat_filename,
                                       std::string* error_msg);

  static uint32_t CalculateCombinedImageChecksum(InstructionSet isa = kRuntimeISA);

 private:
  struct ImageInfo {
    uint32_t oat_checksum = 0;
    uintptr_t oat_data_begin = 0;
    int32_t patch_delta = 0;
    std::string location;
  };

  // Returns the current image location.
  // Returns an empty string if the image location could not be retrieved.
  //
  // TODO: This method should belong with an image file manager, not
  // the oat file assistant.
  static std::string ImageLocation();

  // Gets the dex checksum required for an up-to-date oat file.
  // Returns dex_checksum if a required checksum was located. Returns
  // null if the required checksum was not found.
  // The caller shouldn't clean up or free the returned pointer.
  // This sets the has_original_dex_files_ field to true if a checksum was
  // found for the dex_location_ dex file.
  const uint32_t* GetRequiredDexChecksum();

  // Returns the loaded image info.
  // Loads the image info if needed. Returns null if the image info failed
  // to load.
  // The caller shouldn't clean up or free the returned pointer.
  const ImageInfo* GetImageInfo();

  uint32_t GetCombinedImageChecksum();

  // To implement Lock(), we lock a dummy file where the oat file would go
  // (adding ".flock" to the target file name) and retain the lock for the
  // remaining lifetime of the OatFileAssistant object.
  ScopedFlock flock_;

  std::string dex_location_;

  // In a properly constructed OatFileAssistant object, isa_ should be either
  // the 32 or 64 bit variant for the current device.
  const InstructionSet isa_ = kNone;

  // Cached value of the required dex checksum.
  // This should be accessed only by the GetRequiredDexChecksum() method.
  uint32_t cached_required_dex_checksum_;
  bool required_dex_checksum_attempted_ = false;
  bool required_dex_checksum_found_;
  bool has_original_dex_files_;

  OatFileInfo odex_;
  OatFileInfo oat_;

  // Cached value of the image info.
  // Use the GetImageInfo method rather than accessing these directly.
  // TODO: The image info should probably be moved out of the oat file
  // assistant to an image file manager.
  bool image_info_load_attempted_ = false;
  bool image_info_load_succeeded_ = false;
  ImageInfo cached_image_info_;
  uint32_t combined_image_checksum_ = 0;

  DISALLOW_COPY_AND_ASSIGN(OatFileAssistant);
};

}  // namespace art

#endif  // ART_RUNTIME_OAT_FILE_ASSISTANT_H_
