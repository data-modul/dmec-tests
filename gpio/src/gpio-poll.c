/* vim: set ts=4:sts=4:sw=4:noet:
 *
 * gpio-poll.c - demonstrate catching a GPIO change event without polling
 *               ironically using the poll(2) system call
 *
 * Author: Tim Harvey <tharvey@gateworks.com>
 */

#include <fcntl.h>
#include <time.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/reboot.h>
#include <sys/types.h>
#include <sys/stat.h>

int main(int argc, char **argv)
{
	struct pollfd fdset;
	char path[256];
	char buf[32];
	int gpio;
	int fd, rz, c;

	if (argc < 3) {
		fprintf(stderr, "usage: %s <gpio> <rising|falling|both>\n",
			argv[0]);
		exit(-1);
	}

	gpio = atoi(argv[1]);

	/* configure gpio trigger:
	 *   edge trigger of 'both' (falling and rising) allows catching
	 *   changes in both directions vs level triggerd.
	 */
	sprintf(path, "/sys/class/gpio/gpio%d/edge", gpio);
	if ((fd = open(path, O_WRONLY)) < 0) {
		perror("open() failed\n");
		fprintf(stderr, "non-exported or non-input gpio: %s\n", path);
		exit(-1);
	}
	write(fd, argv[2], strlen(argv[2]));
	close(fd);

	/* open gpio sysfs value for reading in blocking mode */
	sprintf(path, "/sys/class/gpio/gpio%d/value", gpio);
	if ((fd = open(path, O_RDONLY | O_NONBLOCK)) < 0) {
		perror("open() failed\n");
		fprintf(stderr, "invalid or non-exported gpio: %s\n", path);
		exit(-1);
	}

	printf("monitoring %s for interrupt using poll()\n", path);
	while (1) {
		/* use poll(2) to block for 10s or until interrupt occurs */
		fdset.fd = fd;
		fdset.events = POLLPRI;
		fdset.revents = 0;
		if ((c = poll(&fdset, 1, 10000)) < 0) {
			perror("poll() failed");
			break;
		}

		if (fdset.revents & POLLPRI) {
			/* show gpio value */
			lseek(fdset.fd, 0, SEEK_SET);
			rz = read(fdset.fd, buf, sizeof(buf));
			buf[rz ? rz - 1 : 0] = 0;
			printf("gpio%d: %d\n", gpio, atoi(buf));
		}
	}
	close(fd);

	return 0;
}
