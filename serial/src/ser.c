#include <termios.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <getopt.h>
#include <stdarg.h>
#include <time.h>
#include <math.h>
#include <ctype.h>


#define BAUDRATE B115200
#define LOGNAME "ser.log"
#define BILLION  1000000000.0

volatile sig_atomic_t done = 0;

#if 0
static void log_print(FILE *fp, char *fmt, ... )
{
	va_list args, args_tmp;
	struct timespec ts;
	char *fmt_tmp = fmt;

	if (!fp)
		return;

	va_start(args, fmt);
	va_copy(args_tmp, args);
	clock_gettime(CLOCK_REALTIME, &ts);
	fprintf(fp, "[%08lu.%06lu] ",
		    (unsigned long)ts.tv_sec, ts.tv_nsec / 1000);
	fprintf(stdout, "[%08lu.%06lu] ",
			(unsigned long)ts.tv_sec, ts.tv_nsec / 1000);
	vfprintf(fp, fmt_tmp, args_tmp);
	vfprintf(stdout, fmt, args);
	fflush(stdout);
	fsync(fileno(fp));
	va_end(args);
	va_end(args_tmp);
}

/* last byte in buf should be zero */
static void ser_print(FILE *fp, char *buf, size_t bsz)
{
	int i;

	/* filter non print chars without CR and ESC */
	for (i = 0; i < bsz; i++)
		if (!isprint(buf[i]) &&  buf[i] != 0xa && buf[i] != 0x1b)
			buf[i]=' ';

	log_print(fp, "%s", buf);
	fsync(fileno(fp));
}
#endif

static void usage()
{
	printf("\nUsage: ser -d device [-s] -f file\n\n");
	printf("   -d device	serial device\n"
	       "   -s		send\n"
	       "   -f file	complete path name of logile\n"
	       "   Example:\n"
	       "     ser -d /dev/ttyUSB0 \n\n");
}

/* TODO: make this thread safe */
static void term(int signum)
{
    done = 1;
}
static int ser_setup_serial(int fd, struct termios *oldtio)
{
	struct termios newtio;

	tcgetattr(fd, oldtio);

	memset(&newtio, 0, sizeof(newtio));

	newtio.c_cflag = BAUDRATE | CS8 | CLOCAL | CREAD;
	newtio.c_iflag = IGNPAR | IGNCR;
	newtio.c_oflag = ONOCR | OCRNL;
	newtio.c_lflag = ICANON;

	// block for up till 128 characters
	newtio.c_cc[VMIN] = 1;

	// 0.5 seconds read timeout
	newtio.c_cc[VTIME] = 1;

	tcflush(fd, TCIFLUSH);
	tcsetattr(fd, TCSANOW, &newtio);

	return 0;
}

static int ser_read_line(int fd, char *buf, size_t bsz)
{
	fd_set rfds;
	struct timeval tv;

	FD_ZERO(&rfds);
	FD_SET(fd, &rfds);
	tv.tv_sec = 0;
	tv.tv_usec = 500*1000;

	int rv = select(fd+1, &rfds, NULL, NULL, &tv);
	if (0 > rv) {
		perror("select()");
		return -1;
	}
	if (0 < rv) {
		int n = read(fd, buf, bsz-1);
		if (n > 0)
			buf[n] = 0;
		return n;
	}

	return 0;
}

static void ser_send_loop(int fd, const char *lfn)
{
	unsigned int nbs = 0, i = 0;
	FILE *fp = fopen(lfn, "w+");
	if (!fp)
		perror("fopen");

	printf("sending loop\n");
	while(fp && !done) {
		char buf[256];
		snprintf(buf, sizeof(buf)-1, "[%d].deadbeef\n", i++);
		int n = write(fd, buf, strlen(buf));
		if (0 > n) {
			perror("select()");
			break;
		}
		nbs += n;
		printf("bytes sent: %20s bytes: %-40d \n", buf, nbs);
		usleep(50*1000);
	}

	if (fp)
		fclose(fp);
}

static void ser_recv_loop(int fd, const char *lfn)
{
	unsigned int nbr = 0;
	FILE *fp = fopen(lfn, "w+");
	if (!fp)
		perror("fopen");

	printf("recving loop\n");
	while(fp && !done) {
		char buf[256];
		int n = ser_read_line(fd, buf, sizeof(buf)-1);
		if (0 > n) {
			perror("select()");
			break;
		}
		if (n > 0) {
			nbr += n;
			printf("bytes recv'd: %20s bytes: %40d\n", buf, nbr);
		}
	}

	if (fp)
		fclose(fp);
}

int main(int argc, char **argv)
{
	int fd;
	struct termios oldtio;
	struct sigaction action;
	int c, send = 0;
	char *dev = NULL, *logfile = NULL;

	while (1) {
		int option_index = 0;
		static struct option long_options[] = {
			{"d", required_argument, 0, 0},
			{"s", no_argument, 0, 1},
			{"f", required_argument, 0, 2},
			{"help", no_argument, 0, 3},
			{0, 0, 0, 4}
		};

		c = getopt_long(argc, argv, "d:sf:",
				long_options, &option_index);
		if (c == -1)
			break;

		switch (c) {
		case 0:
		case 'd':
			dev = strdup(optarg);
			break;
		case 1:
		case 's':
			send = 1;
			break;
		case 2:
		case 'f':
			logfile = strdup(optarg);
			break;
		case 'h':
		case 4:
		case '?':
		default:
			usage();
		}
		if (optind == 6)
			break;
	}

	if (!dev) {
		usage();
		goto end;
	}

	if (!logfile)
		logfile = strdup(LOGNAME);

	memset(&action, 0, sizeof(struct sigaction));
	action.sa_handler = term;
	sigaction(SIGTERM, &action, NULL);

	fd = open(dev, O_RDWR);
	if (0 > fd) {
		perror("open");
		exit(1);
	}

	ser_setup_serial(fd, &oldtio);

	if (send)
		ser_send_loop(fd, logfile);
	else
		ser_recv_loop(fd, logfile);

	tcsetattr(fd,TCSANOW, &oldtio);

end:
	free(dev);
	free(logfile);

	return 0;
}
