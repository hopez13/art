#include <cstddef>
#include <iostream>

#include "dex/dex_file_loader.h"

extern "C" int LLVMFuzzerTestOneInput(const char* data, size_t size) {
  std::string location = "";
  art::DexFileLoader loader(data, size, location);
  std::string error_msg;

  if (!loader.Open(
          /*location_checksum=*/0, /*verify=*/true, /*verify_checksum=*/false, &error_msg)) {
    std::cout << "ERROR: " << error_msg;
    return 1;
  }
  return 0;
}