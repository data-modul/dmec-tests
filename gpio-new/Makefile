gem-src = $(wildcard gpio-event*.c)
gem-obj = $(gem-src:.c=.o)
gem-dep = $(gem-obj:.o=.d)
gh-src = $(wildcard gpio-hammer*.c)
gh-obj = $(gh-src:.c=.o)
gh-dep = $(gh-obj:.o=.d)
lg-src = $(wildcard lsgpio*.c) $(wildcard gpio-utils*.c)
lg-obj = $(lg-src:.c=.o)
lg-dep = $(lg-obj:.o=.d)

COMPILER = $(CROSS_COMPILE)gcc
CC ?= $(COMPILER)

CFLAGS = -Wall -c -g -fPIC -D_GNU_SOURCE
LDFLAGS = -fPIC -lpthread

ifneq ($(SYSROOT),)
CFLAGS += --sysroot=$(SYSROOT)
LDFLAGS += --sysroot=$(SYSROOT)
endif

all: gpio-event-mon gpio-hammer lsgpio

gpio-event-mon: $(gem-obj)
	$(CC) -o $@ $^ $(LDFLAGS)

gpio-hammer: $(gh-obj)
	$(CC) -o $@ $^ $(LDFLAGS)

lsgpio: $(lg-obj)
	$(CC) -o $@ $^ $(LDFLAGS)

-include $(gem-dep)
-include $(lg-dep)
-include $(gh-dep)

# rule to generate a dep file by using the C preprocessor
# (see man cpp for details on the -MM and -MT options)
%.d: %.c
	@$(CPP) $(CFLAGS) $< -MM -MT $(@:.d=.o) >$@

.PHONY: clean
clean:
	@rm -f *.o *~
	@rm -f $(gem-obj) gpio-event-mon $(gem-dep)
	@rm -f $(gh-obj) gpio-hammer $(gh-dep)
	@rm -f $(lg-obk) lsgpio $(lg-dep)

install: all
	install -m 777 gpio-event-mon $(DESTDIR)
	install -m 777 gpio-hammer $(DESTDIR)
	install -m 777 lsgpio $(DESTDIR)
