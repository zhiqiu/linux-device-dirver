#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <linux/fcntl.h>

int main(){
	int file, res, fpid, i, j;

	char buf_write[1024];
	char buf_read[1024];
	// open
	// open 返回值0,1,2分别对应stdin，stdout，stderr
	if((file = open("/dev/scull0", O_RDWR)) == -1){
		printf("Failed to open /dev/scull0\n");
		return 0;
	}
	printf("Open '/dev/scull0' success, file_id is %d\n", file);

	fpid = fork();

	// 子进程write
	if(fpid == 0){
		sprintf(buf_write, "%s", "abc");
		res = write(file, buf_write, sizeof(buf_write));
		if(res != -1){
			printf("fpid 0: Write to scull0: %s\n", buf_write);
		}
		lseek(file, 0, SEEK_SET);
		res = read(file, buf_read, sizeof(buf_read));
		if(res != -1){
			printf("fpid 0: Read from scull0: %s\n", buf_read);
		}
	}
	// 父进程读
	else{
		sprintf(buf_write, "%s", "cba");
		res = write(file, buf_write, sizeof(buf_write));
		if(res != -1){
			printf("fpid 1: Write to scull0: %s\n", buf_write);
		}
		lseek(file, 0, SEEK_SET);
		res = read(file, buf_read, sizeof(buf_read));
		if(res != -1){
			printf("fpid 1: Read from scull0: %s\n", buf_read);
		}
	}
	// lseek
    // res = lseek(file, 6, SEEK_SET);

	close(file);
	return 0;	
}
