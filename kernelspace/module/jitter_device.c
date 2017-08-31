#include <linux/init.h>
#include <linux/module.h>
#include <linux/cdev.h>
#include <linux/coda.h>
#include <linux/kdev_t.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/string.h>
#include <linux/irqflags.h>
#include <linux/compiler.h>
#include <asm/uaccess.h>

/*buffer size*/
#define SIZE 128
#define COUNT 1

typedef unsigned long long jitter_t;

static int message[SIZE] = {0};
static int index = 0;
static dev_t devid = 0;
static char *chr_name = "jitter_device";
static struct cdev *jitter_cdev;
static int read_lock = 1;

/* read timestamp */
static inline void get_nstime(jitter_t *time) {
        uint32_t high, low;
        asm volatile("rdtsc" : "=a" (low), "=d" (high));
        *time = (jitter_t)high << 32 | low;
}

#define JITTER_IOC_MAGIC 'J'
#define GET_MESSAGE_BUFFER_LEN  _IOR(JITTER_IOC_MAGIC, 1, int)

static ssize_t jitter_read(struct file *fp, char __user *buf,
			size_t len, loff_t *off)
{
	unsigned long offset = *off;
	unsigned int count = len;
	int ret = 0;
	
	while(ACCESS_ONCE(read_lock) == 1) {;}
	if (offset >= sizeof(message))
		return 0;
	if (count > sizeof(message) - offset)
		count = sizeof(message) - offset;
	if(!copy_to_user(buf, message+offset, count*sizeof(int))) {
		*off += count;
		ret = count;
	}else
		return -EFAULT;

	return ret;
}

static int jitter_open(struct inode *ino, struct file *fp) {
	printk("test success\n");
	return 0;
}

static long jitter_ioctl(struct file *fp, unsigned int cmd,
			unsigned long arg)
{
	int ret = 0;
	int __user *p = (int __user *)arg;
	switch(cmd) {
		case GET_MESSAGE_BUFFER_LEN:
			ret = put_user(SIZE, p);
			break;
		default:
			break;
	}
	printk(KERN_INFO"the ioctl return val is:%d\n", ret);

	return ret;
}

static struct file_operations fops = {
	.open = jitter_open,
	.read = jitter_read,
	.unlocked_ioctl = jitter_ioctl
};

static int register_cdev(void) {
        int ret;

        if ((ret=alloc_chrdev_region(&devid, 0, COUNT,
			chr_name)) != 0)
	{
                printk("get major number failed\n");
                return 0;
        }
        jitter_cdev = cdev_alloc();
        jitter_cdev->ops = &fops;
        jitter_cdev->owner = THIS_MODULE;
        ret = cdev_add(jitter_cdev, devid, COUNT);
        if (ret != 0) {
                printk("add cdev to system failed\n");
                return 0;
        }
	return ret;
}

//static int external_loop = 1000;
static int num_warm_loop = 10;

static void collect_jitter(void) {
        int i, j;
        long l = 10;
        long x1, x2, x3, dummy;
        jitter_t time1, time2, jitter_time;

        local_irq_disable();
        for (i = 0; i < SIZE; i++) {
                for (j = 0; j < num_warm_loop; j++) {
                        x1 = l;
                        x2 = x1 * l;
                        x3 = x1 * l;
                        x3--;
                        dummy += x3 / 4;
                }

                get_nstime(&time1);
                x1 = l;
                x2 = x1 * l;
                x3 = x1 * l;
                x3--;
                dummy += x3 / 4;
                get_nstime(&time2);
                jitter_time = time2 -time1;
                message[index++] = (int)jitter_time;
		printk("%d\n", message[index - 1]);
        }
        local_irq_enable();
	read_lock = 0;
	wmb();
}

static void remove_cdev(void) {
	unregister_chrdev_region(devid, COUNT);
	cdev_del(jitter_cdev);
}

static int __init jitter_init(void) {
	int ret = 0;

	ret = register_cdev();
	if (ret != 0) {
		printk(KERN_INFO"register cdev failed\n");
		return 0;
	}
	collect_jitter();

	return 0;
}

static void __exit jitter_exit(void) {
	remove_cdev();
}

module_init(jitter_init);
module_exit(jitter_exit);
