#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kdev_t.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/uaccess.h> //copy_to/from_user()
#include <linux/gpio.h>    //GPIO
#include <linux/err.h>

// character device driver

#define GPIO_5 (5)
#define GPIO_6 (6)

dev_t dev = 0;
static struct class *dev_class;
static struct cdev gpio_cdev;

static int __init gpio_driver_init(void);
static void __exit gpio_driver_exit(void);

/*************** Driver functions **********************/
static int gpio_open(struct inode *inode, struct file *file);
static int gpio_release(struct inode *inode, struct file *file);
static ssize_t gpio_read(struct file *filp, char __user *buf, size_t len, loff_t *off);
static ssize_t gpio_write(struct file *filp, const char *buf, size_t len, loff_t *off);
/******************************************************/

static struct file_operations fops =
    {
        .owner = THIS_MODULE,
        .read = gpio_read,
        .write = gpio_write,
        .open = gpio_open,
        .release = gpio_release,
};

static int gpio_open(struct inode *inode, struct file *file)
{
    pr_info("Device File Opened...!!!\n");
    return 0;
}

static int gpio_release(struct inode *inode, struct file *file)
{
    pr_info("Device File Closed...!!!\n");
    return 0;
}

static ssize_t gpio_read(struct file *filp, char __user *buf, size_t len, loff_t *off)
{
    
    uint8_t gpio_states[2];     // 2bit : 0 1 紀錄GPIO狀態

    // 讀取 GPIO 狀態
    gpio_states[0] = gpio_get_value(GPIO_5);
    gpio_states[1] = gpio_get_value(GPIO_6);

    // 如果 user 要求的長度不足 2，直接拒絕
    if (len < 2)
    {
        pr_err("ERROR: Buffer too small (need 2 bytes)\n");
        return -EINVAL;
    }

    // 複製 GPIO 狀態到 user space
    if (copy_to_user(buf, gpio_states, 2))
    {
        pr_err("ERROR: Failed to copy to user space\n");
        return -EFAULT;
    }

    pr_info("Read GPIO: 5=%d, 6=%d\n",
            gpio_states[0], gpio_states[1]);

    return 2;
}

static ssize_t gpio_write(struct file *filp, const char __user *buf, size_t len, loff_t *off)
{

    uint8_t rec_buf[10] = {0};   // 00 01 10 11

    // 允許傳入 2 或 3 個位元組（包含換行 \n）
    if (len < 2 || len > 3)
    {
        pr_err("ERROR: Expected 2 or 3 bytes, got %zu\n", len);
        return -EINVAL;
    }

    // 嘗試複製前最多 3 個 byte
    if (copy_from_user(rec_buf, buf, len))
    {
        pr_err("ERROR: Failed to copy from user\n");
        return -EFAULT;
    }

    if (len == 3 && rec_buf[2] == '\n')
    {
        rec_buf[2] = '\0';
    }

    if(rec_buf[0] == '1'){
        gpio_set_value(GPIO_5, 1);
        pr_err("GPIO_5 work!!");
    }else{
        gpio_set_value(GPIO_5, 0);
        pr_err("GPIO_5 not work!!");
    }

    if(rec_buf[1] == '1'){
        gpio_set_value(GPIO_6, 1);
        pr_err("GPIO_6 work!!");
    }else{
        gpio_set_value(GPIO_6, 0);
        pr_err("GPIO_6 not work!!");
    }

    return len;
}

static int __init gpio_driver_init(void)
{
    /*Allocating Major number*/
    if ((alloc_chrdev_region(&dev, 0, 1, "gpio_Dev")) < 0)
    {
        pr_err("Cannot allocate major number\n");
        goto r_unreg;
    }
    pr_info("Major = %d Minor = %d \n", MAJOR(dev), MINOR(dev));

    /*Creating cdev structure*/
    cdev_init(&gpio_cdev, &fops);

    /*Adding character device to the system*/
    if ((cdev_add(&gpio_cdev, dev, 1)) < 0)
    {
        pr_err("Cannot add the device to the system\n");
        goto r_del;
    }

    /*Creating struct class*/
    if (IS_ERR(dev_class = class_create(THIS_MODULE, "gpio_class")))
    {
        pr_err("Cannot create the struct class\n");
        goto r_class;
    }

    /*Creating device*/
    if (IS_ERR(device_create(dev_class, NULL, dev, NULL, "gpio_device")))
    {
        pr_err("Cannot create the Device \n");
        goto r_device;
    }

    // Checking the GPIO is valid or not
    if (gpio_is_valid(GPIO_5) == false)
    {
        pr_err("GPIO %d is not valid\n", GPIO_5);
        goto r_device;
    }
    if (gpio_is_valid(GPIO_6) == false)
    {
        pr_err("GPIO %d is not valid\n", GPIO_6);
        goto r_device;
    }

    // Requesting the GPIO
    if (gpio_request(GPIO_5, "GPIO_5") < 0)
    {
        pr_err("ERROR: GPIO %d request\n", GPIO_5);
        goto r_gpio;
    }
    if (gpio_request(GPIO_6, "GPIO_6") < 0)
    {
        pr_err("ERROR: GPIO %d request\n", GPIO_6);
        goto r_gpio;
    }

    // configure the GPIO as output
    gpio_direction_output(GPIO_5, 0);
    gpio_direction_output(GPIO_6, 0);

    /*
    echo 1000 > /dev/test
    echo 0000 > /dev/test
    */
    gpio_export(GPIO_5, false);
    gpio_export(GPIO_6, false);

    pr_info("Device Driver Insert...Done!!!\n");
    return 0;

r_gpio:
    gpio_free(GPIO_5);
    gpio_free(GPIO_6);
r_device:
    device_destroy(dev_class, dev);
r_class:
    class_destroy(dev_class);
r_del:
    cdev_del(&gpio_cdev);
r_unreg:
    unregister_chrdev_region(dev, 1);

    return -1;
}

static void __exit gpio_driver_exit(void)
{
    gpio_unexport(GPIO_5);
    gpio_free(GPIO_5);
    gpio_unexport(GPIO_6);
    gpio_free(GPIO_6);

    device_destroy(dev_class, dev);
    class_destroy(dev_class);
    cdev_del(&gpio_cdev);
    unregister_chrdev_region(dev, 1);
    pr_info("Device Driver Remove...Done!!\n");
}

module_init(gpio_driver_init);
module_exit(gpio_driver_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Yen");
MODULE_DESCRIPTION("GPIO kernel module");
MODULE_VERSION("1");
