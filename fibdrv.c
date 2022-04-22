#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/kdev_t.h>
#include <linux/kernel.h>
#include <linux/ktime.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/string.h>
#include <linux/types.h>

#include "ubn.h"

MODULE_LICENSE("Dual MIT/GPL");
MODULE_AUTHOR("National Cheng Kung University, Taiwan");
MODULE_DESCRIPTION("Fibonacci engine driver");
MODULE_VERSION("0.1");

#define DEV_FIBONACCI_NAME "fibonacci"
#define FIB_MAX 1000

static dev_t fib_dev = 0;
static struct cdev *fib_cdev;
static struct class *fib_class;
static DEFINE_MUTEX(fib_mutex);
static ktime_t kt_fib = 0;
static ktime_t kt_copy = 0;
static int read_opt = 0;

enum { READ_FIB_STR, READ_KT_FIB, READ_KT_COPY };

/* Returns one plus the index of the most significant 1-bit of n */
#define flsll(n) (64 - __builtin_clzll(n))

/* Calculating fibonacci numbers by fast doubling */
/* f(2k) = f(k) * [2 * f(k+1) - f(k)]*/
/* f(2k+1) = f(k)^2 + f(k+1)^2 */
static ubn_t fib_sequence(long long n)
{
    ubn_t a, b, c, d;

    if (!n) {
        ubn_from_extend(&a, (ubn_b_extend_t) 0);
        return a;
    }

    if (n == 1) {
        ubn_from_extend(&a, (ubn_b_extend_t) 1);
        return a;
    }

    /* Starting from f(1), skip the most significant 1-bit */
    int i = (flsll(n) - 1) - 1;

    ubn_from_extend(&a, (ubn_b_extend_t) 1); /* f(1) = 1 */
    ubn_from_extend(&b, (ubn_b_extend_t) 1); /* f(2) = 1 */

    for (int mask = 1 << i; mask; mask >>= 1) {
        ubn_t tmp1, tmp2, tmp3;

        ubn_from_extend(&tmp1, (ubn_b_extend_t) 2); /* tmp1 = 2 */
        ubn_mul(&tmp1, &b, &tmp2);                  /* tmp2 = 2 * b */
        ubn_sub(&tmp2, &a, &tmp3);                  /* tmp3 = 2 * b - a */
        ubn_mul(&a, &tmp3, &c);                     /* c = a * (2 * b - a) */

        ubn_mul(&a, &a, &tmp1);    /* tmp1 = a * a */
        ubn_mul(&b, &b, &tmp2);    /* tmp2 = b * b */
        ubn_add(&tmp1, &tmp2, &d); /* d = a * a + b * b */

        if (n & mask) {
            a = d;
            ubn_add(&c, &d, &b); /* f(2k+2) = f(2k) + f(2k+1) = c + d */
        } else {
            a = c;
            b = d;
        }
    }
    return a;
}

static int fib_open(struct inode *inode, struct file *file)
{
    if (!mutex_trylock(&fib_mutex)) {
        printk(KERN_ALERT "fibdrv is in use");
        return -EBUSY;
    }
    return 0;
}

static int fib_release(struct inode *inode, struct file *file)
{
    mutex_unlock(&fib_mutex);
    return 0;
}

long long kt_fib_long, kt_copy_long;

/* calculate the fibonacci number at given offset */
static ssize_t fib_read(struct file *file,
                        char *buf,
                        size_t size,
                        loff_t *offset)
{
    int ret = 0;
    char str[UBN_STR_SIZE];

    switch (read_opt) {
    case READ_FIB_STR:
        kt_fib = ktime_get();
        ubn_t fib = fib_sequence(*offset);
        ubn_to_str(&fib, str);
        kt_fib = ktime_sub(ktime_get(), kt_fib);
        kt_fib_long = ktime_to_ns(kt_fib);

        unsigned long len = strlen(str) + 1;

        kt_copy = ktime_get();
        ret = copy_to_user((void *) buf, str, len);
        kt_copy = ktime_sub(ktime_get(), kt_copy);
        kt_copy_long = ktime_to_ns(kt_copy);

        break;
    case READ_KT_FIB:
        ret = copy_to_user((void *) buf, &kt_fib_long, sizeof(long long));
        break;
    case READ_KT_COPY:
        ret = copy_to_user((void *) buf, &kt_copy_long, sizeof(long long));
        break;
    default:
        return -1;
    }

    read_opt = (read_opt + 1) % 3;

    return ret;
}

/* write operation is skipped */
static ssize_t fib_write(struct file *file,
                         const char *buf,
                         size_t size,
                         loff_t *offset)
{
    return 1;
}

static loff_t fib_device_lseek(struct file *file, loff_t offset, int orig)
{
    loff_t new_pos = 0;
    switch (orig) {
    case 0: /* SEEK_SET: */
        new_pos = offset;
        break;
    case 1: /* SEEK_CUR: */
        new_pos = file->f_pos + offset;
        break;
    case 2: /* SEEK_END: */
        new_pos = FIB_MAX - offset;
        break;
    }

    if (new_pos > FIB_MAX)
        new_pos = FIB_MAX;  // max case
    if (new_pos < 0)
        new_pos = 0;        // min case
    file->f_pos = new_pos;  // This is what we'll use now
    return new_pos;
}

const struct file_operations fib_fops = {
    .owner = THIS_MODULE,
    .read = fib_read,
    .write = fib_write,
    .open = fib_open,
    .release = fib_release,
    .llseek = fib_device_lseek,
};

static int __init init_fib_dev(void)
{
    int rc = 0;

    mutex_init(&fib_mutex);

    // Let's register the device
    // This will dynamically allocate the major number
    rc = alloc_chrdev_region(&fib_dev, 0, 1, DEV_FIBONACCI_NAME);

    if (rc < 0) {
        printk(KERN_ALERT
               "Failed to register the fibonacci char device. rc = %i",
               rc);
        return rc;
    }

    fib_cdev = cdev_alloc();
    if (fib_cdev == NULL) {
        printk(KERN_ALERT "Failed to alloc cdev");
        rc = -1;
        goto failed_cdev;
    }
    fib_cdev->ops = &fib_fops;
    rc = cdev_add(fib_cdev, fib_dev, 1);

    if (rc < 0) {
        printk(KERN_ALERT "Failed to add cdev");
        rc = -2;
        goto failed_cdev;
    }

    fib_class = class_create(THIS_MODULE, DEV_FIBONACCI_NAME);

    if (!fib_class) {
        printk(KERN_ALERT "Failed to create device class");
        rc = -3;
        goto failed_class_create;
    }

    if (!device_create(fib_class, NULL, fib_dev, NULL, DEV_FIBONACCI_NAME)) {
        printk(KERN_ALERT "Failed to create device");
        rc = -4;
        goto failed_device_create;
    }

    return rc;
failed_device_create:
    class_destroy(fib_class);
failed_class_create:
    cdev_del(fib_cdev);
failed_cdev:
    unregister_chrdev_region(fib_dev, 1);
    return rc;
}

static void __exit exit_fib_dev(void)
{
    // kobject_put(fib_kobj);
    mutex_destroy(&fib_mutex);
    device_destroy(fib_class, fib_dev);
    class_destroy(fib_class);
    cdev_del(fib_cdev);
    unregister_chrdev_region(fib_dev, 1);
}

module_init(init_fib_dev);
module_exit(exit_fib_dev);
