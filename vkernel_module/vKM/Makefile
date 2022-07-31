obj-m += vkernel_hook.o

vkernel_hook-y := main.o hook.o

KVER = `uname -r`
KDIR = /lib/modules/${KVER}/build

all:
	make -C $(KDIR) M=$(PWD) modules

clean:
	make -C $(KDIR) M=$(PWD) clean
