#include <sys/types.h>
#include <stdio.h>
#include <linux/userfaultfd.h>
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
#include <sys/uio.h>
#include <time.h>
#include <poll.h>
#include <stdint.h>
#include <assert.h>

#include <atomic>

#define errExit(msg)    do { perror(msg); exit(EXIT_FAILURE); \
                        } while (0)
#define PAGE_SIZE 4096
static int page_size;
static size_t fault_cnt = 0;
static long uffd;          /* userfaultfd file descriptor */
volatile char c;
uint64_t total_core_work_time = 0, max_work_time = 0, min_work_time = -1;

char* from;
volatile char* addr;

uint64_t NanoTime() {
  struct timespec now;
  clock_gettime(CLOCK_MONOTONIC, &now);
  return (uint64_t)now.tv_sec * 1000000000 + now.tv_nsec;
}

#pragma clang diagnostic push
#  pragma clang diagnostic ignored "-Wframe-larger-than="
static void* fault_handler_thread(void *arg)
{
    static struct uffd_msg msg;   /* Data read from userfaultfd */
    char page[PAGE_SIZE];
    struct uffdio_copy uffdio_copy;
    ssize_t nread;

    uffd = (long) arg;

    /* Loop, handling incoming events on the userfaultfd
       file descriptor */
    for (;;) {
#if 0
        /* See what poll() tells us about the userfaultfd */
        struct pollfd pollfd;
        int nready;
        pollfd.fd = uffd;
        pollfd.events = POLLIN;
        nready = poll(&pollfd, 1, -1);
        if (nready == -1)
          errExit("poll");
        /*
        printf("\nfault_handler_thread():\n");
        printf("    poll() returns: nready = %d; "
                "POLLIN = %d; POLLERR = %d\n", nready,
                (pollfd.revents & POLLIN) != 0,
                (pollfd.revents & POLLERR) != 0);
        */
#endif
        /* Read an event from the userfaultfd */
        nread = read(uffd, &msg, sizeof(msg));
        uint64_t start_time = NanoTime();
        if (nread == 0) {
          printf("EOF on userfaultfd!\n");
          exit(EXIT_FAILURE);
        }

        if (nread == -1)
          errExit("read");

        /* We expect only one kind of event; verify that assumption */
        if (msg.event != UFFD_EVENT_PAGEFAULT) {
          fprintf(stderr, "Unexpected event on userfaultfd\n");
          exit(EXIT_FAILURE);
        }
        atomic_thread_fence(std::memory_order_acquire);

        /* Display info about the page-fault event */
        /*
        printf("    UFFD_EVENT_PAGEFAULT event: ");
        printf("flags = %llx; ", msg.arg.pagefault.flags);
        printf("address = %llx\n", msg.arg.pagefault.address);
        */

        /* Copy the page pointed to by 'page' into the faulting
           region. Vary the contents that are copied in, so that it
           is more obvious that each fault is handled separately. */
        //memset(page, 'A' + fault_cnt % 20, page_size);
        unsigned long offset = ((char*)msg.arg.pagefault.address - addr) & ~(page_size - 1);
        memcpy(page, from + offset, page_size);
        fault_cnt++;

        uffdio_copy.src = (unsigned long) page;

        /* We need to handle page faults in units of pages(!).
           So, round faulting address down to page boundary */
        uffdio_copy.dst = (unsigned long) msg.arg.pagefault.address &
                                           ~(page_size - 1);
        uffdio_copy.len = page_size;
        uffdio_copy.mode = 0;
        uffdio_copy.copy = 0;
        if (ioctl(uffd, UFFDIO_COPY, &uffdio_copy) == -1)
          errExit("ioctl-UFFDIO_COPY");
        atomic_thread_fence(std::memory_order_release);
        /*
        printf("        (uffdio_copy.copy returned %lld)\n",
                uffdio_copy.copy);
        */
        uint64_t cur_time = NanoTime() - start_time;
        if (max_work_time < cur_time)
            max_work_time = cur_time;
        if (min_work_time > cur_time)
            min_work_time = cur_time;
        total_core_work_time += cur_time;
    }
}

#  pragma clang diagnostic ignored "-Wunused-parameter"
void segfault_hdl(int sig, siginfo_t *siginfo, void *context) {
  assert(sig == SIGBUS && context);
  //char *page = NULL;
  struct uffdio_copy uffdio_copy;
  char page[PAGE_SIZE];
  uint64_t start_time = NanoTime();
  atomic_thread_fence(std::memory_order_acquire);

  //memset(page, 'A' + fault_cnt % 20, page_size);
  unsigned long offset = ((char*)siginfo->si_addr - addr) & ~(page_size - 1);
  memcpy(page, from + offset, page_size);
  fault_cnt++;

  uffdio_copy.src = (unsigned long) page;
  /* We need to handle page faults in units of pages(!).
     So, round faulting address down to page boundary */
  uffdio_copy.dst = (unsigned long) ((unsigned long)siginfo->si_addr & ~(page_size - 1));
  uffdio_copy.len = page_size;
  uffdio_copy.mode = UFFDIO_COPY_MODE_DONTWAKE;
  uffdio_copy.copy = 0;
  if (ioctl(uffd, UFFDIO_COPY, &uffdio_copy))
    errExit("ioctl-UFFDIO_COPY");
  atomic_thread_fence(std::memory_order_release);
  uint64_t cur_time = NanoTime() - start_time;
  if (max_work_time < cur_time)
      max_work_time = cur_time;
  if (min_work_time > cur_time)
      min_work_time = cur_time;
  total_core_work_time += cur_time;
}
#pragma clang diagnostic pop

int main(int argc, char *argv[])
{
    unsigned long len;  /* Length of region handled by userfaultfd */
    struct uffdio_api uffdio_api;
    struct uffdio_register uffdio_register;

    if (argc < 3) {
      fprintf(stderr, "Usage: %s <#pages> <#iterations> [0 for fd (default), 1 for SIGBUS]\n", argv[0]);
      exit(EXIT_FAILURE);
    }

    page_size = sysconf(_SC_PAGE_SIZE);
    assert(page_size == PAGE_SIZE);
    len = strtoul(argv[1], NULL, 0) * page_size;
    int iters = strtoul(argv[2], NULL, 0);
    int choice = 0;
    if (argc > 3) {
      choice = strtoul(argv[3], NULL, 0);
    }

    from = (char*) mmap(NULL, len, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    for (unsigned long i = 0; i < len / sizeof(int); i++) {
      *((int*)from + i) = i;
    }

    /* Create and enable userfaultfd object */
    uffd = syscall(__NR_userfaultfd, O_CLOEXEC | UFFD_USER_MODE_ONLY);
    if (uffd == -1)
      errExit("userfaultfd");

    uffdio_api.api = UFFD_API;
    // uffdio_api.features = choice == 0 ? 0 : UFFD_FEATURE_SIGBUS;
    uffdio_api.features = choice == 0 ? 0 : (1 << 7);
    if (ioctl(uffd, UFFDIO_API, &uffdio_api) == -1)
      errExit("ioctl-UFFDIO_API");

    if (choice == 0) {
      /* Create a thread that will process the userfaultfd events */
      pthread_t thr; /* ID of thread that handles page faults */
      int s = pthread_create(&thr, NULL, fault_handler_thread, (void *) uffd);
      if (s != 0) {
        errno = s;
        errExit("pthread_create");
      }
    } else {
      struct sigaction act;
      memset(&act, '\0', sizeof(act));
      act.sa_flags = SA_SIGINFO | SA_RESTART;
      act.sa_sigaction = segfault_hdl;
      if (sigaction(SIGBUS, &act, NULL)) {
        errExit("sigaction-SIGBUS");
      }
    }

    /* Create a private anonymous mapping. The memory will be
       demand-zero paged--that is, not yet allocated. When we
       actually touch the memory, it will be allocated via
       the userfaultfd. */
    addr = (volatile char*) mmap(NULL, len, PROT_READ | PROT_WRITE,
                                 MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (addr == MAP_FAILED)
      errExit("mmap");

    printf("Address returned by mmap() = %p\n", addr);

    /* Register the memory range of the mapping we just created for
       handling by the userfaultfd object. In mode, we request to track
       missing pages (i.e., pages that have not yet been faulted in). */
    uffdio_register.range.start = (unsigned long) addr;
    uffdio_register.range.len = len;
    uffdio_register.mode = UFFDIO_REGISTER_MODE_MISSING;
    if (ioctl(uffd, UFFDIO_REGISTER, &uffdio_register) == -1)
      errExit("ioctl-UFFDIO_REGISTER");

    uint64_t total_time = 0, max_time = 0, min_time = -1;
    for (int i = 0; i < iters; i++) {
    /* Main thread now touches memory in the mapping, touching
       locations 1024 bytes apart. This will trigger userfaultfd
       events for all pages in the region. */
      for (unsigned long l = 0xf; l < len; l += page_size) {
        uint64_t start_time = NanoTime();
        atomic_thread_fence(std::memory_order_acquire);
#if 0
        struct iovec dest = {.iov_base = const_cast<char*>(&c), .iov_len = sizeof(char)};
        struct iovec src = {.iov_base = const_cast<char*>(addr + l), .iov_len = sizeof(char)};
        ssize_t ret = process_vm_readv(getpid(), &dest, 1, &src, 1, 0);
        if (ret == -1) {
          errExit("process_vm_readv");
        }
        if (ret != sizeof(char)) {
          printf("Didn't read the right number of bytes\n");
        }
#endif
        c = addr[l];
        // printf("Read address %p in main(): ", addr + l);
        // printf("%c\n", c);
        // l += 1024;
        // usleep(100000);         /* Slow things down a little */
        atomic_thread_fence(std::memory_order_release);
        // Gather timing info
        uint64_t cur_time = NanoTime() - start_time;
        if (cur_time > max_time) {
          max_time = cur_time;
        }
        if (cur_time < min_time) {
          min_time = cur_time;
        }
        total_time += cur_time;
      }

      if (madvise((void*)addr, len, MADV_DONTNEED)) {
        errExit("madvise");
      }
    }
    unsigned long num_pages = len / page_size;
    printf("Avg time per page: %lu ns.\n", total_time / (num_pages * iters));
    printf("Max time per page: %lu ns.\n", max_time);
    printf("Min time per page: %lu ns.\n", min_time);
    printf("fault_cnt: %lu and core_work: %lu ns.\n", fault_cnt, total_core_work_time / fault_cnt);
    printf("min_core_work: %lu ns. and max: %lu ns.\n", min_work_time, max_work_time);
    exit(EXIT_SUCCESS);
}
