#include <cassert>
#include <chrono>
#include <iostream>
#include <cstring>
#include <random>
#include <array>
#include <atomic>

#include <signal.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/syscall.h>
#include <sys/mman.h>
#include <sys/stat.h>        /* For mode constants */
#include <fcntl.h>           /* For O_* constants */
#include <errno.h>

#define KB (1<<10)
#define MB (KB << 10)
#define GB (MB << 10)
#ifndef PAGE_SIZE
#define PAGE_SIZE (1<<12)
#endif
int main(int, char** argv) {
  static_assert(PAGE_SIZE == 4096);
  const size_t size_in_gb = std::stoul(argv[1]);
  const size_t size = size_in_gb * GB;
  std::chrono::steady_clock::time_point startBench, endBench;
  char* buf = (char*) mmap(NULL, size, PROT_READ | PROT_WRITE,
                           MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  startBench = std::chrono::steady_clock::now();
  for (size_t i = 0; i < size; i += PAGE_SIZE) {
    buf[i] = 'a';
  }
  endBench = std::chrono::steady_clock::now();
  auto timeDiff = std::chrono::duration_cast<std::chrono::milliseconds>(endBench - startBench);
  size_t total_time = timeDiff.count();
  std::cout << "total time:" << total_time
            << " throughput:" << (size_in_gb * KB) / total_time
            << "\n";
  return 0;
}
