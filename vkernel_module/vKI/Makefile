obj-m += vkernel.o

SYSCALLLS_DIR=syscalls/linux

vkernel-y := main.o capability.o apparmor.o syscall.o

KVER = `uname -r`
KDIR = /lib/modules/${KVER}/build

all:
	make -C $(KDIR) M=$(PWD) modules

install:
	mkdir -p /lib/modules/$(KVER)/extra/vkernel
	cp vkernel.ko /lib/modules/$(KVER)/extra/vkernel

clean:
	make -C $(KDIR) M=$(PWD) clean
