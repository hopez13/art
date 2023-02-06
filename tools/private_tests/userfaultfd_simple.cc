#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>

#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/syscall.h>

#include <linux/userfaultfd.h>

void print_api(const struct uffdio_api *api)
{
	printf("api: %llu\n", api->api);
	printf("features: %llu\n", api->features);
	printf("ioctls: %llu\n", api->ioctls);

	printf("\n");
}

int main(void)
{
	long uffd = syscall(__NR_userfaultfd, O_CLOEXEC | O_NONBLOCK);
	if (uffd < 0) {
		perror("syscall(userfaultfd)");
		return -1;
	}

	struct uffdio_api api;
	std::memset(&api, 0x0, sizeof api);
	api.api = UFFD_API;
	if (ioctl(uffd, UFFDIO_API, &api) < 0) {
		perror("UFFDIO_API");
		return -1;
	}

	print_api(&api);

	struct uffd_msg msg;
	std::memset(&msg, 0x0, sizeof msg);
	ssize_t count = read(uffd, &msg, sizeof(msg));
	if (count < 0) {
		perror("read");
		return -1;
	} else if (count == 0) {
		printf("read EOF\n\n");
	}

	printf("read uffd\n\n");

	return 0;
}
