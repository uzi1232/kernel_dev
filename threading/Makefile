obj-m += queue_module.o

MY_VAR := $(shell uname -r)

all:
	make -C /lib/modules/${MY_VAR}/build M=$(PWD) modules

clean:
	make -C /lib/modules/'uname -r'/build M=$(PWD) clean
