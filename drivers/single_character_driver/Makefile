KERNEL_DIR	:=	/lib/modules/$(shell	uname	-r)/build	
PWD	:=	$(shell	pwd)

obj-m	+=	single_schar_driver.o

all:
	$(MAKE)	-C	$(KERNEL_DIR)	M=$(PWD)	modules

clean:
	$(MAKE)	-C	$(KERNEL_DIR)	M=$(PWD)	clean

