#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>

#include <charconv>
#include <iostream>
#include <vector>

std::vector<uint8_t> ReadProcMem(uint64_t pid, uint64_t read_begin, uint64_t read_size) {
  auto open_file_readonly = [](const char* pathname) -> int {
    int res = open(pathname, O_RDONLY, 0);
    if (res < 0) {
      std::cout << "failed to open file: " << pathname;
      exit(-1);
    }
    return res;
  };

  std::string path = "/proc/" + std::to_string(pid) + "/mem";
  const int fd = open_file_readonly(path.c_str());

  std::vector<uint8_t> buf(read_size, 0);
  ssize_t byte_count = pread(fd, buf.data(), read_size, read_begin);
  if (byte_count != static_cast<ssize_t>(read_size)) {
    std::cout << "read byte count mismatch" << std::endl;
    exit(-1);
  }

  int errc = close(fd);
  if (errc != 0) {
    std::cout << "failed to close the file" << std::endl;
    exit(-1);
  }

  return buf;
}

std::vector<uint8_t> ProcessVmReadv(uint64_t pid, uint64_t read_begin, uint64_t read_size) {
  std::vector<uint8_t> buf(read_size, 0);

  struct iovec local_iov;
  local_iov.iov_base = buf.data();
  local_iov.iov_len = read_size;
  unsigned long liovcnt = 1;

  struct iovec remote_iov;
  remote_iov.iov_base = reinterpret_cast<void*>(read_begin);
  remote_iov.iov_len = read_size;
  unsigned long riovcnt = 1;

  unsigned long flags = 0;

  ssize_t vm_readv_res = process_vm_readv(pid,  // remote pid
                                          &local_iov,
                                          liovcnt,
                                          &remote_iov,
                                          riovcnt,
                                          flags);

  if (vm_readv_res == -1) {
    std::cout << "process_vm_readv failed: " << strerror(errno) << std::endl;
    exit(-1);
  }

  return buf;
}

int main(int argc, char** argv) {
  if (argc < 5) {
    std::cout << "Usage: read_proc_mem <pid> <read_begin:hex> <read_end:hex> <type>" << std::endl;
    std::cout << "<type>: proc_mem vm_readv both" << std::endl;
    return -1;
  }

  auto read_int = [](const char* str, int base) -> uint64_t {
    uint64_t value = 0;
    const std::from_chars_result res = std::from_chars(str, str + strlen(str), value, base);
    if (res.ec != std::errc{}) {
      std::cout << "couldn't parse hex: '" << str << "'" << std::endl;
      std::cout << static_cast<int>(res.ec) << std::endl;
      exit(-1);
    }
    return value;
  };

  const uint64_t pid = read_int(argv[1], 10);
  const uint64_t read_begin = read_int(argv[2], 16);
  const uint64_t read_end = read_int(argv[3], 16);
  const std::string type = argv[4];

  std::cout << "pid  : " << pid << std::endl;
  std::cout << "begin: " << read_begin << std::endl;
  std::cout << "end  : " << read_end << std::endl;

  if (read_end <= read_begin) {
    std::cout << "invalid range" << std::endl;
    exit(-1);
  }

  const uint64_t read_size = read_end - read_begin;

  if (type == "proc_mem") {
    const std::vector<uint8_t> buf = ReadProcMem(pid, read_begin, read_size);
    std::cout << "read " << buf.size() << " bytes from /proc/" << pid << "/mem" << std::endl;
  } else if (type == "vm_readv") {
    const std::vector<uint8_t> buf = ProcessVmReadv(pid, read_begin, read_size);
    std::cout << "read " << buf.size() << " bytes with process_vm_readv" << std::endl;
  } else if (type == "both") {
    const std::vector<uint8_t> buf = ReadProcMem(pid, read_begin, read_size);
    std::cout << "read " << buf.size() << " bytes from /proc/" << pid << "/mem" << std::endl;

    const std::vector<uint8_t> buf2 = ProcessVmReadv(pid, read_begin, read_size);
    std::cout << "read " << buf2.size() << " bytes with process_vm_readv" << std::endl;

    size_t mismatch_count = 0;
    for (size_t i = 0; i < buf.size(); ++i) {
      if (buf[i] != buf2[i]) {
        mismatch_count += 1;
      }
    }

    if (mismatch_count != 0) {
      std::cout << "read mem mismatches: " << mismatch_count << std::endl;
      return -1;
    } else {
      std::cout << "no mismatches" << std::endl;
    }
  } else {
    std::cout << "invalid type, must be one of: proc_mem vm_readv both" << std::endl;
    exit(-1);
  }

  return 0;
}
