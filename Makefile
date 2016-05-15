vintage2d-objs := main.o v2d_device.o v2d_context.o common.o
obj-m := vintage2d.o

all:
	make -C /lib/modules/`uname -r`/build M=$(PWD) modules

clean:
	make -C /lib/modules/`uname -r`/build M=$(PWD) clean

