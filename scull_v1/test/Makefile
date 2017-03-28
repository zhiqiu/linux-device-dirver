# 可执行文件  
TARGET = test  

# 依赖目标  
OBJS = test.o  
  
#上面两句可合并成
#test:test.o

# 指令编译器和选项  
CC=gcc  
#CFLAGS=-Wall -std=gnu99  
  
$(TARGET):$(OBJS)  
	$(CC) -o $@ $^  

# $@目标文件，$^所有的依赖文件，$<第一个依赖文件。 
# 缺省认为所有.o都是由对应名字.c编译得到
clean:  
	rm -rf $(TARGET) $(OBJS)  

