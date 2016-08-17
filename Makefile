
SUBDIRS = gpio gpio-new serial i2c watchdog

install: subdirs

.PHONY subdirs: $(SUBDIRS)

$(SUBDIRS):
	$(MAKE) -C $@ install

clean:
	@for d in $(SUBDIRS); do \
		make -C $$d clean; \
	done \

