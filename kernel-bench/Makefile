obj-m += sync.o
sync-objs := sync_test.o barrier.o rlu.o rlu-hash-list.o rcu-hash-list.o rcuintset.o # rluintset.o

CFLAGS_rluintset.o := -DRUN_RLU -DKERNEL
CFLAGS_rcuintset.o := -DRUN_RCU -DKERNEL
CFLAGS_rlu.o := -DKERNEL
CFLAGS_rlu-hash-list.o := -DKERNEL
CFLAGS_sync_test.o := -DKERNEL
# To enable pr_debug/pr_devel in dmesg
#CFLAGS_sync_test.o := -DDEBUG

# V=1 for debug

all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean

info:
	modinfo sync.ko

bench:
	dmesg -c > /dev/null
	insmod sync.ko benchmark="rculist" threads_nb=1 update=0 range=256 duration=500
	rmmod sync.ko
