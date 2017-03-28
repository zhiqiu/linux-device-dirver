#undef PDEBUG /* undef it, just in case */
#ifdef SCULL_DEBUG
# ifdef __KERNEL__
/* This one if debugging is on, and kernel space */
# define PDEBUG(fmt, args...) printk( KERN_DEBUG "scull: " fmt, ## args)
# else
/* This one for user space */
# define PDEBUG(fmt, args...) fprintf(stderr, fmt, ## args)
# endif
#else
# define PDEBUG(fmt, args...) /* not debugging: nothing */
#endif
#undef PDEBUGG #define PDEBUGG(fmt, args...) /* nothing: it's a placeholder */


#ifndef _SCULL_H_  
#define _SCULL_H_

#ifndef SCULL_MAJOR  
#define SCULL_MAJOR 0  
#endif  
 
#ifndef SCULL_NR_DEVS  
#define SCULL_NR_DEVS 4    /* scull0 ~ scull3 */  
#endif  
 
#ifndef SCULL_QUANTUM  
#define SCULL_QUANTUM 4000  
#endif  
 
#ifndef SCULL_QSET  
#define SCULL_QSET  1000  
#endif  

#define TYPE(minor) ((minor)>>4) && 0xff)  
#define NUM(minor)  ((minor) & 0xf)  

extern int scull_major;     /* main.c */  
extern int scull_nr_devs;     
extern int scull_quantum;  
extern int scull_qset;  

struct scull_dev{
	struct scull_qset *data;  //当前节点的数据地址
	int quantum;  //单个量子大小
	int qset;     //每个节点的量子集大小
	unsigned long size;  //节点数量
	unsigned int access_key;
	struct semaphore sem;
	struct cdev cdev;    //字符设备结构
};

struct scull_qset{
	struct scull_qset *next;
	void **data;
};


int scull_open(struct inode *inode, struct file *filep);
int scull_release(struct inode *inode, struct file *filep);
ssize_t scull_read(struct file* filep, char __user* buf, size_t count, loff_t* f_pos);
ssize_t scull_write(struct file* filep, char __user* buf, size_t count, loff_t* f_pos);


int scull_trim(struct scull_dev* dev);
int scull_llseek(struct file* fileop, loff_t off, int whence);
int scull_ioctl(struct inode* inode, struct file* filep, unsigned int cmd, unsigned long arg);


#endif
