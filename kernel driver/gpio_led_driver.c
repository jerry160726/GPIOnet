#include <linux/kernel.h>   // printk()與核心訊息
#include <linux/init.h>     // __init, __exit marco
#include <linux/module.h>   // 模組初始化/卸載相關
#include <linux/kdev_t.h>   // 裝置號管理：major/minor marco
#include <linux/fs.h>        // file_operations結構
#include <linux/cdev.h>     // character device 結構與註冊
#include <linux/device.h>   // device_create, class_create等函式
#include <linux/delay.h>    // mdelay, udelay
#include <linux/uaccess.h>  // copy_to_user, copy_from_user
#include <linux/gpio.h>     // 所有GPIO控制用函式
#include <linux/err.h>      // 錯誤代碼處理

// 指定使用GPIO pin 5和6（BCM編號）
#define GPIO_5 (5)
#define GPIO_6 (6)

dev_t dev = 0;                      // 儲存 major/minor裝置號。
static struct class *dev_class;     // 用來建立/sys/class/與/dev/gpio_device
static struct cdev gpio_cdev;       // character device的核心結構

// 載入模組時執行 / 卸載模組時執行
static int __init gpio_driver_init(void);
static void __exit gpio_driver_exit(void);

/*************** 當/dev/gpio_device被打開、讀寫、關閉時該怎麼做 **********************/
static int gpio_open(struct inode *inode, struct file *file);
static int gpio_release(struct inode *inode, struct file *file);
static ssize_t gpio_read(struct file *filp, char __user *buf, size_t len, loff_t *off);
static ssize_t gpio_write(struct file *filp, const char *buf, size_t len, loff_t *off);
/**********************************************************************************/

// 驅動的對外API表，提供給User space（echo 10 > /dev/gpio_device）使用
static struct file_operations fops =
    {
        .owner = THIS_MODULE,
        .read = gpio_read,
        .write = gpio_write,
        .open = gpio_open,
        .release = gpio_release,
};

// 每次open() / close()裝置檔都會紀錄訊息。
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
    
    uint8_t gpio_states[2];      // 儲存GPIO_5和GPIO_6狀態

    // 讀取實體腳位的電位（0或1）
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

    return 2;       // 成功的話回傳2 bytes：GPIO_5和GPIO_6狀態
}

static ssize_t gpio_write(struct file *filp, const char __user *buf, size_t len, loff_t *off)
{

    uint8_t rec_buf[10] = {0};   // 建立一個緩衝區來儲存使用者送過來的資料（最長處理3 byte，例如"11\n"）

    // 檢查資料長度與內容有效性
    if (len < 2 || len > 3)
    {
        pr_err("ERROR: Expected 2 or 3 bytes, got %zu\n", len);
        return -EINVAL;
    }

    if (copy_from_user(rec_buf, buf, len))
    {
        pr_err("ERROR: Failed to copy from user\n");
        return -EFAULT;
    }

    if (len == 3 && rec_buf[2] == '\n')
    {
        rec_buf[2] = '\0';
    }

    // 第一個字元控制GPIO_5的電位
    if(rec_buf[0] == '1'){
        gpio_set_value(GPIO_5, 1);
        pr_err("GPIO_5 work!!");
    }else{
        gpio_set_value(GPIO_5, 0);
        pr_err("GPIO_5 not work!!");
    }

    //第二個字元控制GPIO_6的電位
    if(rec_buf[1] == '1'){
        gpio_set_value(GPIO_6, 1);
        pr_err("GPIO_6 work!!");
    }else{
        gpio_set_value(GPIO_6, 0);
        pr_err("GPIO_6 not work!!");
    }

    return len;
}

// 初始化函式：載入模組時呼叫
static int __init gpio_driver_init(void)
{
    if ((alloc_chrdev_region(&dev, 0, 1, "gpio_Dev")) < 0)      // 分配裝置號
    {
        pr_err("Cannot allocate major number\n");
        goto r_unreg;
    }
    pr_info("Major = %d Minor = %d \n", MAJOR(dev), MINOR(dev));

    /*註冊character device*/
    cdev_init(&gpio_cdev, &fops);

    /*Adding character device to the system*/
    if ((cdev_add(&gpio_cdev, dev, 1)) < 0)
    {
        pr_err("Cannot add the device to the system\n");
        goto r_del;
    }

    // 在/sys/class/下建立一個名為"gpio_class"的類別資料夾
    // 系統會產生目錄/sys/class/gpio_class/
    if (IS_ERR(dev_class = class_create(THIS_MODULE, "gpio_class")))
    {
        pr_err("Cannot create the struct class\n");
        goto r_class;
    }

    // 在/sys/class/gpio_class/gpio_device/建立sysfs節點
    // 建立 /sys/class/gpio_class/gpio_device/
    // /dev/gpio_device  ← (由udev自動建立)
    if (IS_ERR(device_create(dev_class, NULL, dev, NULL, "gpio_device")))
    {
        pr_err("Cannot create the Device \n");
        goto r_device;
    }

    // 檢查與請求GPIO
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

    // 檢查與請求GPIO
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

// 卸載模組：釋放資源
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

/*這些Marco讓kernel知道載入、卸載的對應函式，同時提供作者資訊與描述，可用modinfo查看*/
module_init(gpio_driver_init);
module_exit(gpio_driver_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Yen");
MODULE_DESCRIPTION("GPIO kernel module");
MODULE_VERSION("1");
