#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/ioctl.h>

typedef int jitter_t;

#define JITTER_IOC_MAGIC 'J'
#define GET_MESSAGE_BUFFER_LEN  _IOR(JITTER_IOC_MAGIC, 1, int)


int main(void) {
	int fd = 0;
	int len = 0;
	int index = 0;
	int *buffer;
	int size = 100000;

    buffer = (int *)malloc(sizeof(int)*size);

	fd = open("/dev/jitter_device", O_RDONLY);
	if (fd < 0) {
		printf("Can't open file\n");
		exit(1);
	}
	if (ioctl(fd, GET_MESSAGE_BUFFER_LEN, &size) < 0) {
		printf("ioctl error\n");
		exit(1);
	}
	len = read(fd, buffer, size);
	if (len < 0) {
		printf("read failed\n");
		//exit(1);
	}
	printf("buffer size is: %d\n", size);
	for (index = 0; index < size; index++) 
		printf("%d\n",buffer[index]);

	return 0;
}
