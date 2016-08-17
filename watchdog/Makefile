wtst-src = $(wildcard watchdog-test*.c)
wtst-obj = $(wtst-src:.c=.o)
wtst-dep = $(wtst-obj:.o=.d)
wsim-src = $(wildcard watchdog-simple*.c)
wsim-obj = $(wsim-src:.c=.o)
wsim-dep = $(wsim-obj:.o=.d)

COMPILER = $(CROSS_COMPILE)gcc
CC := $(COMPILER)

CFLAGS = -Wall -c -g -fPIC
LDFLAGS = -fPIC -lpthread

ifneq ($(SYSROOT),)
CFLAGS += --sysroot=$(SYSROOT)
LDFLAGS += --sysroot=$(SYSROOT)
endif

all: watchdog-test watchdog-simple

watchdog-test: $(wtst-obj)
	$(CC) -o $@ $^ $(LDFLAGS)

watchdog-simple: $(wsim-obj)
	$(CC) -o $@ $^ $(LDFLAGS)

-include $(wtst-dep)
-include $(wsim-dep)

# rule to generate a dep file by using the C preprocessor
# (see man cpp for details on the -MM and -MT options)
%.d: %.c
	@$(CPP) $(CFLAGS) $< -MM -MT $(@:.d=.o) >$@

.PHONY: clean
clean:
	@rm -f *.o *~
	@rm -f $(wtst-obj) watchdog-test $(wtst-dep)
	@rm -f $(wsim-obj) watchdog-simple $(wsim-dep)

install: all
	install -m 777 watchdog-test $(DESTDIR)
	install -m 777 watchdog-simple $(DESTDIR)
