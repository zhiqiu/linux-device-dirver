#include <linux/init.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/fs.h>
#include <linux/proc_fs.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <asm/uaccess.h>

#include "freg.h"

#define DEBUG
#ifdef DEBUG
#define FUNC_LOG()  printk(KERN_EMERG "FUNC_LOG: [%d][%s()]\n", __LINE__, __FUNCTION__)
#endif

// 主从设备号
static int freg_major = 0;
static int freg_minor = 0;

// 设备类别和设备变量
static struct class *freg_class = NULL;
static struct fake_reg_dev *freg_dev = NULL;

// 设备文件操作方法
static int freg_open(struct inode *inode, struct file *filp);
static int freg_release(struct inode *inode, struct file *filp);
static ssize_t freg_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos);
static ssize_t freg_write(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos);

// 设备文件操作方便表
static struct file_operations freg_fops = {
    .owner = THIS_MODULE,
    .open = freg_open,
    .release = freg_release,
    .read = freg_read,
    .write = freg_write,
};

// devfs文件系统的设属性操作方法(接口)
static ssize_t freg_val_show(struct device *dev, struct device_attribute *attr, char *buf);
static ssize_t freg_val_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count);

// devfs文件系统的设备属性
static DEVICE_ATTR(val, S_IRUGO | S_IWUSR, freg_val_show, freg_val_store);

// proc文件系统接口
// static ssize_t freg_proc_read(char *page, char **start, off_t off, int count, int *eof, void *data);
// static ssize_t freg_proc_write(struct file *filp, const char __user *buf, unsigned long len, void *data);

static int freg_open(struct inode *inode, struct file *filp) {
    struct fake_reg_dev *dev;
    dev = container_of(inode->i_cdev, struct fake_reg_dev, dev);
    filp->private_data = dev;
    return 0;
}

static int freg_release(struct inode *inode, struct file *filp) {
    return 0;
}

static ssize_t freg_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos) {
    ssize_t err = 0;
    struct fake_reg_dev *dev = filp->private_data;
    
    if(down_interruptible(&(dev->sem))) {
        return -ERESTARTSYS;
    }

    if(count < sizeof(dev->val)) {
        goto out;
    }

    if(copy_to_user(buf, &(dev->val), sizeof(dev->val))) {
        err = -EFAULT;
        goto out;
    }

    err = sizeof(dev->val);
out:
    up(&(dev->sem));
    return err;
}

static ssize_t freg_write(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos) {
    ssize_t err = 0;
    struct fake_reg_dev *dev = filp->private_data;

    if(down_interruptible(&(dev->sem))) {
        return -ERESTARTSYS;
    }

    if(count != sizeof(dev->val)) {
        goto out;
    }

    if(copy_from_user(&(dev->val), buf, count)) {
        err = -EFAULT;
        goto out;
    }
    err = sizeof(dev->val);
out:
    up(&(dev->sem));
    return err;
}

static ssize_t __freg_get_val(struct fake_reg_dev *dev, char *buf) {
    int val = 0;
    if(down_interruptible(&(dev->sem))) {
        return -ERESTARTSYS;
    }
    val = dev->val;
    up(&(dev->sem));
    return snprintf(buf, PAGE_SIZE, "%d\n", val);
}

static ssize_t __freg_set_val(struct fake_reg_dev *dev, const char *buf, size_t count) {
    int val = 0;
    // 字符串转数字
    val = simple_strtol(buf, NULL, 10);

    if(down_interruptible(&(dev->sem))) {
        return -ERESTARTSYS;
    }
    dev->val = val;
    up(&(dev->sem));
    return count;
}

static ssize_t freg_val_show(struct device *dev, struct device_attribute *attr, char *buf) {
    struct fake_reg_dev *hdev = (struct fake_reg_dev*)dev_get_drvdata(dev);
    return __freg_get_val(hdev, buf);
}

static ssize_t freg_val_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count) {
    struct fake_reg_dev *hdev = (struct fake_reg_dev*)dev_get_drvdata(dev);
    return __freg_set_val(hdev, buf, count);
}



// proc文件系统接口
// static int freg_proc_show(struct seq_file *seq, void *v) {
//     seq_printf(seq, "proc: %d!\n", );
//     return 0;
// }

// static int freg_proc_open(struct inode *inode, struct file *file) {
//     return single_open(file, freg_proc_show, inode->i_private);
// }

// 从proc文件中读取数据需要经历三个步骤
// 1.申请内存页用于存储数据
// 2.调用文件相关的函数填充步骤1申请的内存页
// 3.把数据从内核空间拷贝到用户空间
static ssize_t freg_proc_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos) {

    ssize_t err = 0;
    struct fake_reg_dev *dev = freg_dev;
    
    if(down_interruptible(&(dev->sem))) {
        return -ERESTARTSYS;
    }

    if(*f_pos > sizeof(dev->val)) {
        goto out;
    }  
    if(count < sizeof(dev->val)) {
        goto out;
    }

    // struct inode *inode = file->f_path.dentry->d_inode;
    // char     *page;
    // struct proc_dir_entry *entry;
    // unsigned long long pos;
    // 。。。
    // dp = PDE(inode); 
    // //此处便是步骤1,获调用函数获取空闲的内存页
    // if (!(page = (char*) __get_free_page(GFP_TEMPORARY)))
    //     return -ENOMEM;
    // //步骤2,选择合适的函数
    // while ((nbytes > 0) && !eof) {
    //     。。。
    //     if (dp->get_info) {
    //         /* Handle old net routines */
    //         n = dp->get_info(page, &start, *ppos, count);
    //         if (n < count)
    //             eof = 1;
    //     } else if (dp->read_proc) {
    //         n = dp->read_proc(page, &start, *ppos,
    //                   count, &eof, dp->data);
    //     } else
    //         break;
    //     。。。
    //     //步骤3,将数据从内核空间拷贝到用户空间
    //      n -= copy_to_user(buf, start < page ? page : start, n);
    //     。。。
    // }
    // free_page((unsigned long) page);
    // return retval;



    if(copy_to_user(buf, &(dev->val), sizeof(dev->val))) {
        err = -EFAULT;
        goto out;
    }
    printk(KERN_ALERT "%d\n", dev->val);
    FUNC_LOG() ;
    err = sizeof(dev->val);
out:
    up(&(dev->sem));
    *f_pos += err;
    return err;
}

static ssize_t freg_proc_write(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos) {
    struct inode *inode = filp->f_path.dentry->d_inode;
    struct proc_dir_entry *entry;
    entry = PDE(inode);

    if (!entry->write_proc)
        return -EIO;
    return entry->write_proc(file, buffer, count, entry->data);
}

// static ssize_t freg_write_proc(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos) {
//     struct inode *inode = filp->f_path.dentry->d_inode;
//     struct proc_dir_entry *entry;
//     entry = PDE(inode);

//     if (!entry->write_proc)
//         return -EIO;
//     return entry->write_proc(file, buffer, count, entry->data);
// }

// static ssize_t freg_read_proc(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos) {
//     struct inode *inode = filp->f_path.dentry->d_inode;
//     struct proc_dir_entry *entry;
//     entry = PDE(inode);

//     if (!entry->write_proc)
//         return -EIO;
//     return entry->write_proc(file, buffer, count, entry->data);
// }

// 设备文件操作表
static const struct file_operations freg_proc_fops = {
    .owner = THIS_MODULE,
    .read  = proc_file_read,
    .write  = freg_proc_write,
};

// 创建/proc/freg文件
static void freg_create_proc(void) {
    struct proc_dir_entry *entry;
    // entry = create_proc_entry(FREG_DEVICE_PROC_NAME, 0, NULL);
    // create_proc_entry removed 
    entry = proc_create(FREG_DEVICE_PROC_NAME, S_IRUGO | S_IRUGO, NULL, &freg_proc_fops);
    if(entry) {
        entry->owner = THIS_MODULE;
        entry->read_proc = freg_read_read;
        entry->write_proc = freg_write_write;
    }
}

static void freg_remove_proc(void) {
    remove_proc_entry(FREG_DEVICE_PROC_NAME, NULL);
}

// 初始化设备
static int __freg_setup_dev(struct fake_reg_dev *dev) {
    int err;
    dev_t devno = MKDEV(freg_major, freg_minor);
    memset(dev, 0, sizeof(struct fake_reg_dev));

    cdev_init(&(dev->dev), &freg_fops);
    dev->dev.owner = THIS_MODULE;
    dev->dev.ops = &freg_fops;

    err = cdev_add(&(dev->dev), devno, 1);
    if(err) {
        return err;
    }
    sema_init(&(dev->sem), 1);
    dev->val = 0;
    return 0;
}
static int __init freg_init(void) {
    int err = -1;
    dev_t dev = 0;
    struct device *temp = NULL;
    printk(KERN_ALERT "Init freg device.\n");

    // 动态分配主从设备号
    err = alloc_chrdev_region(&dev, 0, 1, FREG_DEVICE_NODE_NAME);
    if(err < 0) {
        printk(KERN_ALERT "Failed to alloc char dev region.\n");
        goto fail;
    }

    freg_major = MAJOR(dev);
    freg_minor = MINOR(dev);
    
    // 分配freg设备结构体内存
    freg_dev = kmalloc(sizeof(struct fake_reg_dev), GFP_KERNEL);
    if(!freg_dev) {
        err = -ENOMEM;
        printk(KERN_ALERT "Failed to alloc freg dev.\n");
        goto unregister;
    }
    // 初始化freg
    err = __freg_setup_dev(freg_dev);
    if(err) {
        printk(KERN_ALERT "Failed to setup freg device.\n");
        goto cleanup;
    }

    // 创建/sys/calss下的类别目录freg
    freg_class = class_create(THIS_MODULE, FREG_DEVICE_CLASS_NAME);
    if(IS_ERR(freg_class)) {
        err = PTR_ERR(freg_class);
        printk(KERN_ALERT "Failed to create freg proc class.\n");
        goto destroy_cdev;
    }
    
    // 在/dev/和/sys/calss/freg/下的分别创建设备文件freg
    temp = device_create(freg_class, NULL, dev, NULL, "%s", FREG_DEVICE_FILE_NAME);
    if(IS_ERR(temp)) {
        err = PTR_ERR(temp);
        printk(KERN_ALERT "Failed to create freg device.\n");
        goto destroy_class;
    }

    // 在/sys/class/freg/freg/目录下创建属性文件val
    err = device_create_file(temp, &dev_attr_val);
    if(err < 0) {
        printk(KERN_ALERT "Failed to create attribute val of freg device.\n");
        goto destroy_device;
    }

    dev_set_drvdata(temp, freg_dev);

    // 创建/proc/freg文件
    freg_create_proc();
    printk(KERN_ALERT "Succedded init freg device.\n");

    return 0;

destroy_device:
    device_destroy(freg_class, dev);
destroy_class:
    class_destroy(freg_class);
destroy_cdev:
    cdev_del(&(freg_dev->dev));
cleanup:
    kfree(freg_dev);
unregister:
    unregister_chrdev_region(MKDEV(freg_major, freg_minor), 1);
fail:
    return err;
}


// 模块卸载方法
static void __exit freg_exit(void) {
    dev_t devno = MKDEV(freg_major, freg_minor);
    printk(KERN_ALERT "Destroy freg device.\n");

    freg_remove_proc();

    if(freg_class) {
        device_destroy(freg_class, devno);
        class_destroy(freg_class);
    }
    if(freg_dev) {
        cdev_del(&(freg_dev->dev));
        kfree(freg_dev);
    }
    unregister_chrdev_region(devno, 1);
}

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Fake Register Driver");

module_init(freg_init);
module_exit(freg_exit);
