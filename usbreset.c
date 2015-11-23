#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <linux/usbdevice_fs.h>

int main(int argc, char **argv) {
	char *filename;

	int fd;

	if (argc < 2 ) {
		printf("Missing filename\n");
		return -1;
	}

	filename = argv[1];

	fd = open(filename, O_WRONLY);
	
	if (fd < 0) {
		perror("Failed to open file ");
		return -1;
	}

	if (ioctl(fd, USBDEVFS_RESET, 0) == -1) {
		perror("IOCTL/reset failed ");
		return -1;
	}

	printf("Reset finished\n");

	close(fd);

	return 0;

}
