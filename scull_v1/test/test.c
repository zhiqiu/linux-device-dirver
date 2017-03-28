#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <linux/fcntl.h>

int main(){
	int file, res;

	char buf_write[] = "hello wrold scull\n";
	char buf_read[1024];
	// open
	// open 返回值0,1,2分别对应stdin，stdout，stderr
	if((file = open("/dev/scull0", O_RDWR)) == -1){
		printf("Failed to open /dev/scull0\n");
		return 0;
	}
	printf("Open '/dev/scull0' success, file_id is %d\n", file);
	
	// write
	res = write(file, buf_write, sizeof(buf_write));
	if(res != -1){
		printf("Write to scull0: %s\n", buf_write);
	}
	// lseek
    res = lseek(file, 6, SEEK_SET);
	printf("lseek to: %d\n", res);

	res = read(file, buf_read, sizeof(buf_write));
	if(res != -1){
		printf("Read from scull0: %s \n", buf_read);
	}
	close(file);
	return 0;	
}
