DIRS    := src
SOURCES := $(foreach dir, $(DIRS), $(wildcard $(dir)/*.c))
OBJS    := $(patsubst %.c, %.o, $(SOURCES))
OBJS    := $(foreach o,$(OBJS),./obj/$(o))
DEPFILES:= $(patsubst %.o, %.P, $(OBJS))

CFLAGS = -Wall -MMD -c -g -Isrc -fPIC
LDFLAGS = -fPIC

ifeq ($(ARCH), arm)
CFLAGS += -mfloat-abi=hard
LDFLAGS += -mfloat-abi=hard
endif

ifneq ($(SYSROOT),)
CFLAGS += --sysroot=$(SYSROOT)
LDFLAGS += --sysroot=$(SYSROOT)
endif

PRG = gpio-test

all: main

CC ?= $(CROSS_COMPILE)gcc

# link the executable
main: $(OBJS)
	$(CC) $(OBJS) $(LDFLAGS) -ldl -o $(PRG)

#generate dependency information and compile
obj/%.o : %.c
	mkdir -p $(@D)
	$(CC) $(CFLAGS) -o $@ -MF obj/$*.P $<
#   make needs double $$ in order sed to work properly!
	@sed -e 's/#.*//' -e 's/^[^:]*: *//' -e 's/ *\\$$/\n/' < obj/$*.P | fmt -1 | \
	sed -e '/^$$/ d' -e 's/$$/ :/' >> obj/$*.P;

#remove all generated files
dep_files := $(foreach dir, $(LIB_DIRS) $(DIRS), $(wildcard $(dir)/*.d))
tmp_files := $(foreach dir, $(LIB_DIRS) $(DIRS), $(wildcard $(dir)/*~))

clean:
	@rm -f $(PRG)
	@rm -rf obj
	@rm -rf $(dep_files) $(tmp_files)

install: all
	@install -m 777 $(PRG) $(DESTDIR)

#include the dependency information
-include $(DEPFILES)

-include $(LIB_DEPFILES)
