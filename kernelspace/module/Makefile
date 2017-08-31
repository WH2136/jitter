obj-m := jitter_device.o

PWD := $(shell pwd)

KERNEL := /lib/modules/$(shell uname -r)/build

all:
	make -C $(KERNEL)  M=$(PWD) modules
clean:
	make -C $(KERNEL)  M=$(PWD) clean
rmmod:
	sudo rmmod mychrdev
insmod:
	sudo insmod mychrdev.ko
