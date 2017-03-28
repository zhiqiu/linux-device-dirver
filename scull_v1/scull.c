#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <asm/uaccess.h>

#include <linux/init.h>
#include <linux/module.h>
#include <linux/moduleparam.h>

#include "scull.h"


int scull_major = SCULL_MAJOR;
int scull_minor = 0;
int scull_nr_devs = SCULL_NR_DEVS;
int scull_quantum = SCULL_QUANTUM;
int scull_qset = SCULL_QSET;

//insmod 命令行指定参数
module_param(scull_major, int, S_IRUGO);
module_param(scull_minor, int, S_IRUGO);
module_param(scull_nr_devs, int, S_IRUGO);

MODULE_LICENSE("Dual BSD/GPL");
MODULE_AUTHOR("Chenql");

struct scull_dev *scull_devices;

int scull_open(struct inode *inode, struct file *filep){
	struct scull_dev *dev;
	dev = container_of(inode->i_cdev, struct scull_dev, cdev);
	filep -> private_data = dev;
	
	printk(KERN_EMERG "scull: open\n");

	// 如果只写，则清0
	if((filep->f_flags & O_ACCMODE) == O_WRONLY){
		scull_trim(dev);
	}
	return 0;
}

int scull_release(struct inode *inode, struct file *filep){
	printk(KERN_EMERG "scull: release\n");
	return 0;
}

//指针向下走n
struct scull_qset* scull_follow(struct scull_dev* dev, int n){
	struct scull_qset* qs = dev->data;
	if(!qs){
		qs = dev->data = kmalloc(sizeof(struct scull_qset), GFP_KERNEL);
		if(qs == NULL){
			return NULL;
		}
		memset(qs, 0, sizeof(struct scull_qset));
	}
	while(n--){
		if(!qs->next){
			qs->next = kmalloc(sizeof(struct scull_qset), GFP_KERNEL);
			if(qs->next == NULL){
				return NULL;
			}
			memset(qs->next, 0, sizeof(struct scull_qset));
		}
		qs = qs->next;
	}
	return qs;
}

ssize_t scull_read(struct file* filep, char __user* buf, size_t count, loff_t* f_pos){
	struct scull_dev *dev = filep->private_data;
	struct scull_qset *iter;
	int quantum = dev->quantum, qset = dev->qset;
	int itemsize = quantum * qset;
	int item, s_pos, q_pos, rest;
	ssize_t retval = 0;


	printk(KERN_EMERG "scull: read, count = %d\n", count);

//  linux设备驱动程序书中第三章代码没有写初始化sem，直接使用sem会出错
//	if(down_interruptible(&dev->sem)){
//		return -ERESTARTSYS;
//	}
	if(*f_pos >= dev->size){
		goto out;
	}
	if(*f_pos + count >= dev->size){
		count = dev->size - *f_pos;
	}

	//定位到第一个byte
	item = (long)*f_pos / itemsize; //第几个qset
	rest = (long)*f_pos % itemsize; //qset中的count
	s_pos = rest / quantum;         //qset中第几个quantum
	q_pos = rest % quantum;			//quantum中剩余量，第几个byte开始写
	
	iter = scull_follow(dev, item);
	if(iter == NULL || !iter->data || !iter->data[s_pos]){
		goto out;
	}
	// 只读到quantum末端
	if(count > quantum - q_pos){
		count = quantum - q_pos;
	}
	if(copy_to_user(buf, iter->data[s_pos] + q_pos, count)){
		retval = -EFAULT;
		goto out;
	}
	*f_pos += count;
	retval = count;

out:
//	up(&dev->sem);
	return retval;
}


ssize_t scull_write(struct file* filep, char __user* buf, size_t count, loff_t* f_pos){
	struct scull_dev *dev = filep->private_data;
	struct scull_qset *iter;
	int quantum = dev->quantum, qset = dev->qset;
	int itemsize = quantum * qset;
	int item, s_pos, q_pos, rest;
	ssize_t retval = -ENOMEM;


	printk(KERN_EMERG "scull: write, count = %d\n", count);

//	if(down_interruptible(&dev->sem)){
//		return -ERESTARTSYS;
//	}

	//定位到第一个byte
	item = (long)*f_pos / itemsize; //第几个qset
	rest = (long)*f_pos % itemsize; //qset中的count
	s_pos = rest / quantum;         //qset中第几个quantum
	q_pos = rest % quantum;			//quantum中剩余量，第几个byte开始写
	
	iter = scull_follow(dev, item);
	if(iter == NULL){
		goto out;
	}
	if(!iter->data){
		iter->data = kmalloc(qset*sizeof(char*), GFP_KERNEL);
		if(!iter->data){
			goto out;
		}
		memset(iter->data, 0, qset*sizeof(char *));
	}
	if(!iter->data[s_pos]){
		iter->data[s_pos] = kmalloc(quantum, GFP_KERNEL);
		printk(KERN_EMERG "scull: write, kmalloc, address = %d\n", iter->data[s_pos]);
		if(!iter->data[s_pos]){
			goto out;
		}
	}

	// 只写到quantum末端
	if(count > quantum - q_pos){
		count = quantum - q_pos;
	}
	if(copy_from_user(iter->data[s_pos] + q_pos, buf, count)){
		retval = -EFAULT;
		goto out;
	}
	*f_pos += count;
	retval = count;
	//更新size，如果有必要
	if(dev->size < *f_pos){
		dev->size = *f_pos;
	}
out:
//	up(&dev->sem);
	return retval;
}


int scull_trim(struct scull_dev* dev){
	struct scull_qset* next, *iter;
	int qset = dev->qset;
	int i;
	for(iter = dev->data; iter; iter = next){
		if(iter->data){
			for(i = 0; i < qset; i++){
				kfree(iter->data[i]);
			}
			kfree(iter->data);
			iter->data = NULL;
		}
		next = iter->next;
		kfree(iter);
	}
	dev->size = 0;
	dev->quantum = scull_quantum;
	dev->qset = scull_qset;
	dev->data = NULL;
	return 0;
}

int scull_llseek(struct file* filep, loff_t off, int whence){
	struct scull_dev *dev = filep->private_data;
	loff_t newpos;



	switch(whence){
		case 0: //SEEK_SET
			newpos = off;
			break;
		case 1: //SEEK_CUR
			newpos = filep->f_pos + off;
			break;
		case 2:
			newpos = dev->size +off;
			break;
		default:
			return -EINVAL;
	}
	if(newpos < 0){
		return -EINVAL;  
	}
	
	printk(KERN_EMERG "scull: llseek, newpos = %d\n", newpos);
	
	
	filep->f_pos = newpos;
	return newpos;
}
int scull_ioctl(struct inode* inode, struct file* filep, unsigned int cmd, unsigned long arg){
	//TODO
}

struct file_operations scull_fops = {
	.owner = THIS_MODULE,
	.open = scull_open,
	.release = scull_release,
	.read = scull_read,
	.write = scull_write,
	.llseek = scull_llseek,
	//.ioctl = scull_ioctl,
};

void scull_cleanup(){
	int i;
	dev_t devno = MKDEV(scull_major, scull_minor);
	if(scull_devices){
		for(i = 0; i < scull_nr_devs; i++){
			scull_trim(scull_devices + i);
			cdev_del(&scull_devices[i].cdev);
		}
		kfree(scull_devices);
	}
	unregister_chrdev_region(devno, scull_nr_devs);
}

static void scull_setup_cdev(struct scull_dev *dev, int index){
	int err, devno = MKDEV(scull_major, scull_minor + index);
	cdev_init(&dev->cdev, &scull_fops);
	dev->cdev.owner = THIS_MODULE;
	dev->cdev.ops = &scull_fops;
	err = cdev_add(&dev->cdev, devno, 1);
	
	if(err){
		printk(KERN_NOTICE "Error %d adding scull %d", err, index);
	}
}

int scull_init(){
	int res, i;
	dev_t dev = 0;
	
	//设备号的动态获取
	if(scull_major){
		dev = MKDEV(scull_major, scull_minor);
		res = register_chrdev_region(dev, scull_nr_devs, "scull");
	}
	else{
		res = alloc_chrdev_region(&dev, scull_minor, scull_nr_devs, "scull");
		scull_major = MAJOR(dev);
	}

	if(res < 0){
		printk(KERN_WARNING "scull: can't get major %d\n", scull_major);
		return res;
	}

	scull_devices = kmalloc(scull_nr_devs * sizeof(struct scull_dev), GFP_KERNEL);
	if(!scull_devices){
		res = -ENOMEM;
		goto fail;
	}
	memset(scull_devices, 0, scull_nr_devs * sizeof(struct scull_dev));
	printk(KERN_ALERT "The length of scull_dev is %d",sizeof(struct scull_dev));

	for(i = 0; i < scull_nr_devs; i++){
		scull_devices[i].quantum = scull_quantum;
		scull_devices[i].qset = scull_qset;
		scull_setup_cdev(&scull_devices[i], i);
	}

	dev = MKDEV(scull_major, scull_minor + scull_nr_devs);
	return 0;

fail:
	scull_cleanup();
	return res;
}

module_init(scull_init);
module_exit(scull_cleanup);
