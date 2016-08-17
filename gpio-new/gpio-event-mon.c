/*
 * gpio-hammer - example swiss army knife to shake GPIO lines on a system
 *
 * Copyright (C) 2016 Linus Walleij
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 *
 * Usage:
 *	gpio-event-mon -n <device-name> -o <offset>
 */
#include <unistd.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <dirent.h>
#include <errno.h>
#include <string.h>
#include <poll.h>
#include <fcntl.h>
#include <getopt.h>
#include <signal.h>
#include <inttypes.h>
#include <sys/ioctl.h>
#include <linux/gpio.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/syscall.h>
#include <sys/epoll.h>

#define NGPIO	8

static int thread_stop;

struct gpio_params {
	int fd;
	unsigned int gpios[NGPIO];
	unsigned int ngpio;
	u_int32_t handleflags[NGPIO];
	u_int32_t eventflags[NGPIO];
};

static inline long gettid()
{
	return syscall(SYS_gettid);
}

static int gpio_dev_open(const char *device_name)
{
	char *chrdev_name;
	int ret;

	ret = asprintf(&chrdev_name, "/dev/%s", device_name);
	if (ret < 0)
		return -ENOMEM;

	int fd = open(chrdev_name, 0);

	free(chrdev_name);
	return fd;
}

static int gpio_setup_in_line(int fd,
			      int line,
			      u_int32_t handleflags,
			      u_int32_t eventflags)
{
	struct gpioevent_request req;
	struct gpiohandle_data data;
	int ret;

	printf("eflags: %#x\n", eventflags);
	req.lineoffset = line;
	req.handleflags = handleflags;
	req.eventflags = eventflags;
	strcpy(req.consumer_label, "gpio-event-mon");

	ret = ioctl(fd, GPIO_GET_LINEEVENT_IOCTL, &req);
	if (ret == -1) {
		ret = -errno;
		fprintf(stderr, "Failed to issue GET EVENT "
			"IOCTL (%d)\n",
			ret);
		return ret;;
	}

	/* Read initial states */
	ret = ioctl(req.fd, GPIOHANDLE_GET_LINE_VALUES_IOCTL, &data);
	if (ret == -1) {
		ret = -errno;
		fprintf(stderr, "Failed to issue GPIOHANDLE GET LINE "
			"VALUES IOCTL (%d)\n",
			ret);
		return ret;
	}

	fprintf(stdout, "Monitoring line %d\n", line);
	fprintf(stdout, "Initial line value: %d\n", data.values[0]);

	return req.fd;
}

static int gpio_read_sta(int efd, unsigned long *cnt)
{
	struct gpioevent_data event;
	int ret = -1;

	while(ret < 0) {
		ret = read(efd, &event, sizeof(event));
		if (ret == -1) {
			if (errno == -EAGAIN) {
				fprintf(stderr, "nothing available\n");
				continue;
			} else {
				ret = -errno;
				fprintf(stderr, "Failed to read event (%d)\n",
					ret);
				break;
			}
		}
	}

	if (ret != sizeof(event)) {
		fprintf(stderr, "Reading event failed\n");
		return -EIO;
	}

	unsigned int tid = gettid();
	fprintf(stdout, "[%u]: GPIO EVENT %l" PRIu64 ": ",
		tid, event.timestamp);
	switch (event.id) {
	case GPIOEVENT_EVENT_RISING_EDGE:
		fprintf(stdout, "rising edge");
		break;
	case GPIOEVENT_EVENT_FALLING_EDGE:
		fprintf(stdout, "falling edge");
		break;
	default:
		fprintf(stdout, "unknown event");
	}
	fprintf(stdout, " -> cnt=%lu\n", (*cnt)++);

	return ret;
}

static int monitor_device(int fd,
		   unsigned int line,
		   u_int32_t handleflags,
		   u_int32_t eventflags,
		   unsigned int loops)
{
	int ret = 0, efd;
	int i = 0;
	unsigned long cnt = 0;

	efd = gpio_setup_in_line(fd, line, handleflags, eventflags);

	while (fd > 0 && !thread_stop) {
		ret = gpio_read_sta(efd, &cnt);
		if (ret < 0)
			break;

		i++;
		if (i == loops)
			break;
	}

	return ret;
}

void print_usage(void)
{
	fprintf(stderr, "Usage: gpio-event-mon [options]...\n"
		"Listen to events on GPIO lines, 0->1 1->0\n"
		"  -n <name>  Listen on GPIOs on a named device (must be stated)\n"
		"  -o <n>     Offset to monitor\n"
		"  -d         Set line as open drain\n"
		"  -s         Set line as open source\n"
		"  -r         Listen for rising edges\n"
		"  -f         Listen for falling edges\n"
		" [-c <n>]    Do <n> loops (optional, infinite loop if not stated)\n"
		"  -?         This helptext\n"
		"\n"
		"Example:\n"
		"gpio-event-mon -n gpiochip0 -o 4 -r -f\n"
	);
}


static void *gpio_thread(void *arg)
{
	struct gpio_params *p = (struct gpio_params *) arg;
	struct epoll_event ev, events[NGPIO];
	int ret = 0, efd[NGPIO];
	unsigned long cnt[NGPIO];
	int i = 0;

	int epollfd = epoll_create1(0);

	if (!p->ngpio)
		return NULL;

	printf("gpios: %d\n", p->ngpio);
	for (i = 0; i < p->ngpio; i++) {
		efd[i] = gpio_setup_in_line(p->fd,
					    p->gpios[i],
					    p->handleflags[i],
					    p->eventflags[i]);
		ev.events = EPOLLIN;
		ev.data.fd = efd[i];
		if (epoll_ctl(epollfd, EPOLL_CTL_ADD, efd[i], &ev) == -1) {
			perror("epoll_ctl failed");
			ret = -errno;
			thread_stop = 1;
		}
		cnt[i] = 0;
		fprintf(stdout, "line %d configured.\n", p->gpios[i]);
	}

	while (!thread_stop) {
		int nfds = epoll_wait(epollfd, events, p->ngpio, -1);
		if (nfds == -1 && errno == EINTR) {
			perror("epoll_wait");
			continue;
		}
		if (nfds == -1 && errno != EINTR) {
			perror("epoll_wait");
			ret = -errno;
			break;
		}

		for (i = 0; i < nfds; i++) {
			ret = gpio_read_sta(events[i].data.fd, &cnt[i]);
			if (ret < 0)
				break;
		}
	}

	printf("thread %lu stop\n", gettid());
	pthread_exit(NULL);

}

static void term(int sig)
{
	thread_stop = 1;
}



static void gpio_init_params(struct gpio_params *p,
			     u_int32_t handleflags,
			     u_int32_t eventflags)
{
	int i;

	for (i = 0; i < NGPIO; i++) {
		p->handleflags[i] = handleflags;
		p->eventflags[i] = eventflags;
	}
}

int main(int argc, char **argv)
{
	const char *device_name = NULL;
	unsigned int line = -1, i;
	unsigned int loops = 0;
	u_int32_t handleflags = GPIOHANDLE_REQUEST_INPUT;
	u_int32_t eventflags = 0;
	int c, multi_thread = 0;

	while ((c = getopt(argc, argv, "c:n:o:dsrfm?")) != -1) {
		switch (c) {
		case 'c':
			loops = strtoul(optarg, NULL, 10);
			break;
		case 'n':
			device_name = optarg;
			break;
		case 'o':
			line = strtoul(optarg, NULL, 10);
			break;
		case 'd':
			handleflags |= GPIOHANDLE_REQUEST_OPEN_DRAIN;
			break;
		case 's':
			handleflags |= GPIOHANDLE_REQUEST_OPEN_SOURCE;
			break;
		case 'r':
			eventflags |= GPIOEVENT_REQUEST_RISING_EDGE;
			break;
		case 'f':
			eventflags |= GPIOEVENT_REQUEST_FALLING_EDGE;
			break;
		case 'm':
			multi_thread = 1;
			break;
		case '?':
			print_usage();
			return -1;
		}
	}

	if (!device_name || line == -1) {
		print_usage();
		return -1;
	}
	if (!eventflags) {
		printf("No flags specified, listening on both rising and "
		       "falling edges\n");
		eventflags = GPIOEVENT_REQUEST_BOTH_EDGES;
	}

	static struct gpio_params params = { 0 };
	static struct gpio_params p[NGPIO];

	gpio_init_params(&params, handleflags, eventflags);

	printf("main line: %d\n", line);
	for (i = optind; i < argc && params.ngpio < NGPIO; i++) {
		params.gpios[params.ngpio++] = strtoul(argv[i], NULL, 10);
		printf("additional line: %d\n", params.gpios[params.ngpio-1]);
	}
	signal(SIGINT, term);

	int fd = gpio_dev_open(device_name);
	if (fd < 0) {
		perror("Failed open gpio device");
		exit(-1);
	}

	params.fd = fd;

	if (!multi_thread) {
		pthread_t t;
		void *res;

		pthread_create(&t, NULL, &gpio_thread, (void *)&params);
		monitor_device(fd, line, handleflags, eventflags, loops);
		pthread_join(t, &res);
	} else {
		pthread_t t[NGPIO];
		for (i = 0; i < params.ngpio; i++) {
			p[i].fd = params.fd;
			p[i].gpios[0] = params.gpios[i];
			p[i].ngpio = 1;
			p[i].eventflags[0] = eventflags;
			p[i].handleflags[0] = handleflags;
			pthread_create(&t[i], NULL, &gpio_thread, &p[i]);
		}

		monitor_device(fd, line, handleflags, eventflags, loops);
		for (i = 0; i < params.ngpio; i++) {
			void *res;
			pthread_join(t[i], &res);
		}
	}

	if (close(fd) == -1)
		perror("Failed to close GPIO character device file");

	return 0;
}
