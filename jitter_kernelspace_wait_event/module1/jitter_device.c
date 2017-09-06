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
#include <linux/wait.h>


/*buffer size*/
#define READ_DATA_SIZE 100000
#define SIZE 100
#define COUNT 1

typedef unsigned long long jitter_t;

static unsigned has_valid_data = 0;
static int message[SIZE] = {0};
static int index = 0;
static dev_t devid = 0;
static char *chr_name = "jitter_device";
static struct cdev *jitter_cdev;

/* read timestamp */
static inline void get_nstime(jitter_t *time) {
        uint32_t high, low;
        asm volatile("rdtsc" : "=a" (low), "=d" (high));
        *time = (jitter_t)high << 32 | low;
}

#define JITTER_IOC_MAGIC 'J'
#define GET_MESSAGE_BUFFER_LEN  _IOR(JITTER_IOC_MAGIC, 1, int)

DECLARE_WAIT_QUEUE_HEAD(reader);
DECLARE_WAIT_QUEUE_HEAD(writer);

static ssize_t jitter_read(struct file *fp, char __user *buf,
			size_t len, loff_t *off)
{
	unsigned long offset = *off;
	int ret = 0;
    int i = 0;
    int user_buffer_offset = 0;
	
	if (offset >= SIZE)
	    return 0;
    for (i = 0; i < READ_DATA_SIZE/SIZE; i++) {
        wait_event_interruptible(reader, has_valid_data == 1);
        user_buffer_offset = i*SIZE*sizeof(int);
	    if(!copy_to_user(buf+user_buffer_offset, message, SIZE*sizeof(int))) {
		    ret += user_buffer_offset;
	    } else {
		    return -EFAULT;
        }
        has_valid_data = 0;
        wmb();
        wake_up_interruptible(&writer);
    }
    *off = SIZE;

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
			ret = put_user(READ_DATA_SIZE, p);
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
        int i, j, has_read = 0;
        long l = 10;
        long x1, x2, x3, dummy;
        jitter_t time1, time2, jitter_time;
        while(has_read <= READ_DATA_SIZE) {
            wait_event_interruptible(writer, has_valid_data == 0);
            printk("start write\n");
            index = 0;
            wmb();
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
            }
            local_irq_enable();
	        has_valid_data = 1;
            has_read += SIZE;
	        wmb();
            wake_up_interruptible(&reader);
        }
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
MODULE_LICENSE("GPL");
