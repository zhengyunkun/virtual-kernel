obj-m += vkernel.o

SYSCALLS_DIR=syscalls/linux
FUTEX_DIR=$(SYSCALLS_DIR)/futex
TRACE_DIR=syscalls/trace

vkernel-y := main.o capability.o apparmor.o syscall.o
vkernel-y += $(TRACE_DIR)/kallsyms.o $(TRACE_DIR)/trace_syscalls.o
vkernel-y += ${FUTEX_DIR}/futex.o ${FUTEX_DIR}/kallsyms.o ${FUTEX_DIR}/page_alloc.o

KVER = `uname -r`
KDIR = /lib/modules/${KVER}/build

all:
	make -C $(KDIR) M=$(PWD) modules

install:
	mkdir -p /lib/modules/$(KVER)/extra/vkernel
	cp vkernel.ko /lib/modules/$(KVER)/extra/vkernel

clean:
	make -C $(KDIR) M=$(PWD) clean
