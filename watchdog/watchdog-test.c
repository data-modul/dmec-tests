/* vim: set ts=4:sts=4:sw=4:noet: */
/*
 * Watchdog Driver Test Program
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <getopt.h>
#include <linux/types.h>
#include <linux/watchdog.h>

#define DEFAULT_TIMEOUT		5

static int fd;
static int stop;
static int force;
static int win_mode;

static struct watchdog_info wdt_info;

static void print_watchdog_info(struct watchdog_info *wdi)
{
	printf("\n===========\n");
	printf("options: %#x\n", wdi->options);
	printf("fw ver.: %#x\n", wdi->firmware_version);
	printf("identity: %s\n", wdi->identity);
}

static void usage(void)
{
	printf("\nUsage: watchdog-test [OPTION]...\n\n");
	printf("   -d device        device name\n");
	printf("   -t timeout       watchdog timeout\n");
	printf("   -s stop          stop watchdog on exit\n");
	printf("   -p pretimeout          watchdog pretimeout\n");
	printf("   -w window mode   enable watchdog window mode\n");
	printf("Example:\n");
	printf("\twatchdog_test -d /dev/watchdog1\n\n");
}

/*
 * This function simply sends an IOCTL to the driver, which in turn ticks
 * the PC Watchdog card to reset its internal timer so it doesn't trigger
 * a computer reset.
 */
static void keep_alive(void)
{
	int dummy, ret;

	ret = ioctl(fd, WDIOC_KEEPALIVE, &dummy);
	if (ret)
		printf("error: %d\n", ret);
}

/*
 * The main program.  Run the program with "-d" to disable the card,
 * or "-e" to enable the card.
 */

static void term(int sig)
{
	if (stop) {
		fprintf(stderr, "WARN: stopping watchdog ticks...\n");
		if (win_mode) {
			int flags = WDIOS_DISABLECARD;

			ioctl(fd, WDIOC_SETOPTIONS, &flags);
		} else {
			int n = write(fd, "V", sizeof("V"));

			if (n <= 0)
				printf("write failed %d\n", n);
		}
		}

	close(fd);

	exit(0);
}

static void watchdog_loop_standard(int fd,
								   int timeout,
								   int pretimeout)
{
	unsigned long i = 0;
	long ping_time;

	int ret = ioctl(fd, WDIOC_SETPRETIMEOUT, &pretimeout);
	if (ret)
		printf("error: setting pretimeout failed %d\n", ret);

	ret = ioctl(fd, WDIOC_GETPRETIMEOUT, &pretimeout);
	if (ret)
		printf("error: getting pretimeout failed %d\n", ret);

	ret = ioctl(fd, WDIOC_SETTIMEOUT, &timeout);
	if (ret)
		printf("error: setting timeout failed %d\n", ret);

	ret = ioctl(fd, WDIOC_GETTIMEOUT, &timeout);
	if (ret)
		printf("error: getting timeout failed %d\n", ret);
	printf("pretimeout: %d timeout: %d\n", pretimeout, timeout);

	if (pretimeout)
		ping_time = (long)(pretimeout - 2);
	else
		ping_time = (long)(timeout - 2);

	if (ping_time <= 0)
			ping_time = 100 * 1000;
	else
			ping_time *= 1000 * 1000;

	printf("keep alive interval: %ld [us]\n", ping_time);
	while (1) {
		int delta = 0, status;

		keep_alive();

		if (ioctl(fd, WDIOC_GETTIMELEFT, &delta) == 0 && delta > 0)
			printf("delta = %#x\n", delta);

		if (ioctl(fd, WDIOC_GETSTATUS, &status))
			printf("error: get status failed %d\n", status);
		printf("[%20lu] status: %#x\n", i++, status);
		usleep(ping_time);
	}

}

static void watchdog_loop_window(int fd,
								 int timeout,
								 int pretimeout)
{
	unsigned int i, flags;
	long ping_time;

	flags = WDIOS_DISABLECARD;
	ioctl(fd, WDIOC_SETOPTIONS, &flags);

	int ret = ioctl(fd, WDIOC_SETPRETIMEOUT, &pretimeout);
	if (ret)
		printf("error: setting pretimeout failed %d\n", ret);

	ret = ioctl(fd, WDIOC_GETPRETIMEOUT, &pretimeout);
	if (ret)
		printf("error: getting pretimeout failed %d\n", ret);

	ret = ioctl(fd, WDIOC_SETTIMEOUT, &timeout);
	if (ret)
		printf("error: setting timeout failed %d\n", ret);

	ret = ioctl(fd, WDIOC_GETTIMEOUT, &timeout);
	if (ret)
		printf("error: getting timeout failed %d\n", ret);
	printf("pretimeout: %d timeout: %d\n", pretimeout, timeout);

	ping_time = pretimeout + timeout / 2;
	if (ping_time == pretimeout)
			ping_time++;

	ping_time *= 1000 * 1000;

	printf("win mode> keep alive interval: %lu [us]\n", ping_time);
	flags = WDIOS_ENABLECARD;
	ioctl(fd, WDIOC_SETOPTIONS, &flags);

	while (1) {
		int status;

		if (ioctl(fd, WDIOC_GETSTATUS, &status))
			printf("error: get status failed %d\n", status);
		printf("[%20d] status: %#x\n", i++, status);
		usleep(ping_time);
		keep_alive();
		if (i > 2 && force)
				keep_alive();
	}
}

int main(int argc, char *argv[])
{
	int flags;
	unsigned int timeout = DEFAULT_TIMEOUT, pretimeout = 0;
	char *watchdog_dev = NULL;

	while (1) {
		int option_index = 0;
		static struct option long_options[] = {
			{"d", required_argument, 0, 0},
			{"t", required_argument, 0, 1},
			{"s", no_argument, 0, 2},
			{"p", required_argument, 0, 3},
			{"w", no_argument, 0, 4},
			{"f", no_argument, 0, 5},
			{"help", no_argument, 0, 6},
			{0, 0, 0, 0}
		};

		char c = getopt_long(argc, argv, "d:ht:sp:wf",
				     long_options, &option_index);
		if (c == -1)
			break;

		switch (c) {
		case 0:
		case 'd':
			watchdog_dev = strdup(optarg);
			break;
		case 1:
		case 't':
			timeout = atoi(optarg);
			break;
		case 2:
		case 's':
			stop = 1;
			break;
		case 3:
		case 'p':
			pretimeout = atoi(optarg);
			break;
		case 4:
		case 'w':
			win_mode = 1;
			break;
		case 5:
		case 'f':
			force = 1;
			break;
		case 6:
		case 'h':
		case '?':
		default:
			usage();
		}
	}

	if (!watchdog_dev) {
		usage();
		goto end;
	}

	if (win_mode && !pretimeout) {
		printf("error: window mode requeires pretimeout.\n");
		usage();
		goto end;
	}

	printf("watchdog device: %s timeout: %d/%d stop: %d win_mode: %d\n",
	       watchdog_dev, pretimeout, timeout, stop, win_mode);

	fd = open(watchdog_dev, O_WRONLY);
	if (fd == -1) {
		fprintf(stderr, "Watchdog device not enabled.\n");
		fflush(stderr);
		exit(-1);
	}

	fprintf(stderr, "\nWatchdog Ticking Away!\n");
	fflush(stderr);

	signal(SIGINT, term);

	int ret = ioctl(fd, WDIOC_GETSUPPORT, &wdt_info);
	if (!ret)
			print_watchdog_info(&wdt_info);

	ret = ioctl(fd, WDIOC_GETBOOTSTATUS, &flags);
	if (!ret)
			printf("boot status: %#x\n", flags);

	if (!win_mode)
		watchdog_loop_standard(fd, timeout, pretimeout);
	else
		watchdog_loop_window(fd, timeout, pretimeout);

end:
	close(fd);
	free(watchdog_dev);
	return 0;
}
