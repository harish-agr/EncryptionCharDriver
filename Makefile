obj-m := cryptctl.o 

all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules
	gcc my_ioctl.c -o app

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
	rm -f app

cryptctl: cryptctl.c
	cc -o cryptctl cryptctl.c


