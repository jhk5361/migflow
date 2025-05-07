# Makefile

obj-m += page_tracker.o
page_tracker-objs := ./page_tracker_main.o ./pebs_sampling.o

all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) CONFIG_DEBUF_INFO_BTF=n modules

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean


