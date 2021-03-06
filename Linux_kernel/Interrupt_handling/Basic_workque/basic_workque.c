/*
 * basic_workque.c
 *
 * Description: Basic linux driver to demostrate interrupt handling in workque
 *              Registring request_irq(), workque, mutex usage with read function
 *
 *  Created on: 03-Mar-2020
 *      Author: Sunil Sharma P <sunilsharma.pv@gmail.com>
 *
 *     Version: 1.0v
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/fs.h> /* Included for character device driver support */
#include <linux/uaccess.h> /* Included for copy_to_user() */
#include <linux/cdev.h> /* Included for 2.6 or higher character driver support */
#include <linux/device.h> /* For device_create() api */
#include <linux/interrupt.h> /* Included for interrupt handler support, and bottom half support. */
#include <linux/workqueue.h> /* Included for work queue support */

MODULE_LICENSE("GPL");
MODULE_VERSION("1.0");
MODULE_AUTHOR("Sunil Sharma P <sunilsharma.pv@gmail.com>");
MODULE_DESCRIPTION("A simple device driver - Interrupts with Workque");

/* Interrupt line */
/* On my laptop, IRQ number 1 corresponds to keyboard controller,
   which generates interrupt whenever any key is pressed. */
#define INTERRUPT_LINE 1

/* Buffer to hold the driver data */
char basic_workque_buffer[256];
int basic_workque_buffer_length = 0;

/* To hold the major number for the device */
dev_t basic_workque_dev = 0;

/* Structure to hold the character driver information */
struct cdev basic_workque_cdev;

/* To hold the class for the device */
static struct class *basic_workque_class;

/* Mutex to avoid a race condition during read() function call */
struct mutex basic_read_mutex;

/* Structure to hold the wait queue for putting an application to sleep */
wait_queue_head_t basic_wait_queue;

/* Structure to hold the work information */
struct work_struct basic_work;

ssize_t basic_workque_read(struct file *pFile, char __user *pBuffer, size_t length, loff_t *pOffset)
{
    printk("Inside the %s() function\n", __FUNCTION__);

    /* Check whether the driver has any data to be returned to the called. If not, put the caller to sleep. */
    if (basic_workque_buffer_length == 0) {

        /* If wait_event_interruptible() returns a negative value, it means the sleep was interrupted */
        if (wait_event_interruptible(basic_wait_queue, (basic_workque_buffer_length > 0)) < 0) {
            /* Since the program was workqueed from sleep, return 0 (EOF) */
            return 0;
        }
    }

    /* Take the mutex before copying the data */
    mutex_lock_interruptible(&basic_read_mutex);

    /* memcpy(dst, str, length) */
    copy_to_user(pBuffer, basic_workque_buffer, basic_workque_buffer_length);
    length = basic_workque_buffer_length;
    basic_workque_buffer_length = 0;

    /* Release the mutex */
    mutex_unlock(&basic_read_mutex);

    return length;
}

void basic_workque_handler(struct work_struct *work)
{
    printk("Inside the %s() function\n", __FUNCTION__);

    /* Take the mutex before copying the data */
    mutex_lock_interruptible(&basic_read_mutex);

    /* Copy the user buffer to the driver buffer */
    memset(basic_workque_buffer, 0, 256);

    /* Generate some data */
    sprintf(basic_workque_buffer, "Workque data generated @ jiffies = %li\n", jiffies);
    basic_workque_buffer_length = strlen(basic_workque_buffer) + 1 /* Null Termination */;

    /* Release the mutex */
    mutex_unlock(&basic_read_mutex);

    /* Since some data has arrived, wake up any pending read */
    wake_up_interruptible(&basic_wait_queue);
}

irqreturn_t basic_workque_irq_handler(int irq_number /* This will be first parameter of request_irq */,
        void *irq_param /* This will be last parameter of request_irq */)
{
    printk("\nInside the %s() function\n", __FUNCTION__);

    /* Schedule/Deffer the work */
    schedule_work(&basic_work);

    return IRQ_HANDLED;
}

int basic_workque_open(struct inode *pInode, struct file *pFile)
{
    printk("Inside the %s() function\n", __FUNCTION__);

    /* Initialize the mutex */
    mutex_init(&basic_read_mutex);

    /* Initialize the work to be done in response to the interrupt */
    INIT_WORK(&basic_work, basic_workque_handler);

    /* Register the workque handler */
    /* request_irq(unsigned int irq, irq_handler_t handler, unsigned long flags, const char *name, void *dev) */
    request_irq(INTERRUPT_LINE,
            basic_workque_irq_handler,
            IRQF_SHARED, /* Because on my laptop, IRQ 1 is already used by key board controller, so sharing with that */
            "basic_workque_irq_hdlr",
            &basic_workque_buffer);

    return 0;
}

int basic_workque_close(struct inode *pInode, struct file *pFile)
{
    printk("Inside the %s() function\n", __FUNCTION__);

    /* Delete / unregister the workque handler */
    free_irq(INTERRUPT_LINE, &basic_workque_buffer);

    /* Flush any work that is pending */
    flush_work(&basic_work);

    /* Destroy the mutex */
    mutex_destroy(&basic_read_mutex);

    return 0;
}

/* Define the file operations structure indicating the functions to be called when the device is accessed */
struct file_operations basic_workque_file_ops = {
    .owner   = THIS_MODULE,
    .open    = basic_workque_open,
    .read    = basic_workque_read,
    .release = basic_workque_close,
};

int basic_workque_init(void)
{
    printk("Inside the %s() function\n", __FUNCTION__);

    /* Initialize the wait queue used for putting an application to sleep */
    init_waitqueue_head(&basic_wait_queue);

    /* Initialize the driver buffer */
    sprintf(basic_workque_buffer, "Buffer for module %s\n", __FILE__);
    basic_workque_buffer_length = strlen(basic_workque_buffer) + 1 /* For Null Termination */;

    /* Inform the kernel about your character device driver */
    /* Register the character device driver using the 2.6 or higher style of interface */
    /* extern int alloc_chrdev_region(dev_t *, unsigned, unsigned, const char *); */
    alloc_chrdev_region(&basic_workque_dev,
            0 /* First Minor Number for the device */,
            1 /* Number of instances */,
            "basic_workque_drv");

    /* Register the file operations for the major number / minor number device for 'n' number of instances */
    /* void cdev_init(struct cdev *, const struct file_operations *); */
    cdev_init(&basic_workque_cdev, &basic_workque_file_ops);

    /* Link the major / minor number of the device, 'n' instance of the device and the file operations */
    /* int cdev_add(struct cdev *, dev_t, unsigned); */
    cdev_add(&basic_workque_cdev,
            basic_workque_dev /* First Device */,
            1 /* Number of Instances */);

    /*Creating struct class*/
    basic_workque_class = class_create(THIS_MODULE, "basic_workque_class");

    /*Creating device, this will apper in /dev files system, does somethig like `mknod` */
    device_create(basic_workque_class,
            NULL,
            basic_workque_dev,
            NULL,
            "basic_workque_device");

    return 0;
}

void basic_workque_exit(void)
{
    printk("Inside the %s() function\n", __FUNCTION__);

    /* Inform the kernel that you are not going to handle the device any more */
    device_destroy(basic_workque_class, basic_workque_dev);

    /* Inform the kernel that you are not going to handle the class any more */
    class_destroy(basic_workque_class);

    /* Inform the kernel that you are not going to handle the character driver any more */
    /* Delete the device */
    /* void cdev_del(struct cdev *); */
    cdev_del(&basic_workque_cdev);

    /* Release the major / minor number region */
    /* extern void unregister_chrdev_region(dev_t, unsigned); */
    unregister_chrdev_region(basic_workque_dev, 1 /* Number of instances */);
}

module_init( basic_workque_init);
module_exit( basic_workque_exit);
