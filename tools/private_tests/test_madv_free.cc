#include <sys/types.h>
#include <stdio.h>
#include <pthread.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <signal.h>
#include <poll.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <sys/ioctl.h>
#include <poll.h>

#include <chrono>
#include <cstring>
#include <cassert>
#include <iostream>

#ifndef MADV_FREE
#define MADV_FREE 8
#endif
#ifndef PAGE_SIZE
#define PAGE_SIZE 4096
#endif

#define GB (1<<30)
int main(int, char* argv[]) {
  static_assert(PAGE_SIZE == 4096);
  const size_t size_in_gb = std::stoul(argv[1]);
  const size_t size = size_in_gb * GB;
  int advice = std::stoul(argv[2]) == 0 ? MADV_FREE : MADV_DONTNEED;
  char* buf = (char*) mmap(NULL, size, PROT_READ | PROT_WRITE,
                          MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  // madvise(buf, SIZE, MADV_HUGEPAGE);
  for (size_t i = 0; i < size; i+= PAGE_SIZE) {
    buf[i] = 'a';
  }
  madvise(buf, size, advice);
  std::cout << "Waiting forever\n";
  while(true) {
    sleep(1000000);
  }
}
