obj-m := cs4624.o

# 如果有多个源文件
# obj-m := hello.o
# hello-objs := file1.o file2.o file3.o


KERNELDIR = /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)
default:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules

# -C表示到存放内核的目录执行其makefile
# M=表示到存放内核的目录执行其makefile
# modules表示编译成模块的意思
# 内核目录下的scripts/Makefile.modpost，定义了make modules
# 实际上的编译指令都在内核目录下

clean:
#	rm -rf *.ko *.o *.mod* *.symvers *.order  
#   clean也可以调用内核目录下的脚本
	$(MAKE) -C $(KERNELDIR) M=$(PWD) clean
