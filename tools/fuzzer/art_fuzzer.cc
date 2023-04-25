#include "base/mem_map.h"
#include "dex/dex_file_loader.h"

extern "C" int LLVMFuzzerTestOneInput(const char* data, size_t size) {
  // Initialize environment
  art::MemMap::Init();

  // Open and verify the DEX file. Do not verify the checksum as we only care about the DEX file
  // contents, and know that the checksum would probably be erroneous.
  std::string error_msg;
  art::DexFileLoader loader((const uint8_t*)data, size, /*location=*/"");
  std::unique_ptr<const art::DexFile> dex_file = loader.Open(
      /*location_checksum=*/0, /*verify=*/true, /*verify_checksum=*/false, &error_msg);
  return 0;
}
