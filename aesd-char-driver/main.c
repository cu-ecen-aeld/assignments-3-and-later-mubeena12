/**
 * @file aesdchar.c
 * @brief Functions and data related to the AESD char driver implementation
 *
 * Based on the implementation of the "scull" device driver, found in
 * Linux Device Drivers example code.
 *
 * @author Dan Walkes
 * @date 2019-10-22
 * @copyright Copyright (c) 2019
 *
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/printk.h>
#include <linux/types.h>
#include <linux/cdev.h>
#include <linux/fs.h> // file_operations
#include <linux/slab.h>
#include "aesdchar.h"
int aesd_major =   0; // use dynamic major
int aesd_minor =   0;

MODULE_AUTHOR("Mubeena Udyavar Kazi"); /** TODO: fill in your name **/
MODULE_LICENSE("Dual BSD/GPL");

struct aesd_dev aesd_device;

int aesd_open(struct inode *inode, struct file *filp)
{
    struct aesd_dev *dev;

    PDEBUG("open");
    /**
     * TODO: handle open
     */
    dev = container_of(inode->i_cdev, struct aesd_dev, cdev);
    filp->private_data = dev;

    return 0;
}

int aesd_release(struct inode *inode, struct file *filp)
{
    PDEBUG("release");
    /**
     * TODO: handle release
     */
    return 0;
}

ssize_t aesd_read(struct file *filp, char __user *buf, size_t count,
                loff_t *f_pos)
{
    ssize_t retval = 0;
    /**
     * TODO: handle read
     */
    struct aesd_dev *dev = filp->private_data;
    size_t entry_offset;
    struct aesd_buffer_entry *entry;

    PDEBUG("read %zu bytes with offset %lld",count,*f_pos);

    entry = aesd_circular_buffer_find_entry_offset_for_fpos(&dev->circular_buffer, *f_pos, &entry_offset);
    if (entry == NULL) {
        retval = 0; // End of file reached, return 0
        goto exit;
    }


    if (entry != NULL) {
        size_t available_bytes = entry->size - entry_offset;
        size_t bytes_to_read = count > available_bytes ? available_bytes : count;

        if (copy_to_user(buf, entry->buffptr + entry_offset, bytes_to_read)) {
            retval = -EFAULT; // Error while copying data to user space
        } else {
            *f_pos += bytes_to_read; // Update file position
            retval = bytes_to_read; // Set return value to the actual number of bytes read
        }
    }

exit:
    return retval;
}

ssize_t aesd_write(struct file *filp, const char __user *buf, size_t count,
                loff_t *f_pos)
{
    ssize_t retval = -ENOMEM;
    struct aesd_dev *dev = filp->private_data;
    size_t bytes_to_write = count;

    char *temp_buf;
    struct aesd_buffer_entry new_entry;
    PDEBUG("write %zu bytes with offset %lld",count,*f_pos);
    /**
     * TODO: handle write
     */

    if (count == 0) {
        goto exit; // Nothing to write
    }

    temp_buf = kmalloc(bytes_to_write, GFP_KERNEL); // Allocate memory for the buffer
    if (!temp_buf) {
        retval = -ENOMEM; // Memory allocation failed
        goto exit; // Jump to cleanup and return
    }

    if (copy_from_user(temp_buf, buf, bytes_to_write)) {
        retval = -EFAULT; // Error while copying data from user space
        goto exit; // Jump to cleanup and return
    }

    new_entry.buffptr = temp_buf;
    new_entry.size = bytes_to_write;
    aesd_circular_buffer_add_entry(&dev->circular_buffer, &new_entry);

    retval = bytes_to_write; // Set return value to the actual number of bytes written

exit:
    return retval;
}
struct file_operations aesd_fops = {
    .owner =    THIS_MODULE,
    .read =     aesd_read,
    .write =    aesd_write,
    .open =     aesd_open,
    .release =  aesd_release,
};

static int aesd_setup_cdev(struct aesd_dev *dev)
{
    int err, devno = MKDEV(aesd_major, aesd_minor);

    cdev_init(&dev->cdev, &aesd_fops);
    dev->cdev.owner = THIS_MODULE;
    dev->cdev.ops = &aesd_fops;
    err = cdev_add (&dev->cdev, devno, 1);
    if (err) {
        printk(KERN_ERR "Error %d adding aesd cdev", err);
    }
    return err;
}



int aesd_init_module(void)
{
    dev_t dev = 0;
    int result;
    result = alloc_chrdev_region(&dev, aesd_minor, 1,
            "aesdchar");
    aesd_major = MAJOR(dev);
    if (result < 0) {
        printk(KERN_WARNING "Can't get major %d\n", aesd_major);
        return result;
    }
    memset(&aesd_device,0,sizeof(struct aesd_dev));

    /**
     * TODO: initialize the AESD specific portion of the device
     */
    // Initialize the circular buffer
    aesd_circular_buffer_init(&aesd_device.circular_buffer);
    
    result = aesd_setup_cdev(&aesd_device);

    if( result ) {
        unregister_chrdev_region(dev, 1);
    }
    return result;

}

void aesd_cleanup_module(void)
{
    struct aesd_buffer_entry *entry;
    uint8_t index;
    dev_t devno = MKDEV(aesd_major, aesd_minor);

    cdev_del(&aesd_device.cdev);

    /**
     * TODO: cleanup AESD specific poritions here as necessary
     */
    AESD_CIRCULAR_BUFFER_FOREACH(entry, &aesd_device.circular_buffer, index) {
        if (entry->buffptr) {
            kfree(entry->buffptr);
            entry->buffptr = NULL;
        }
    }

    unregister_chrdev_region(devno, 1);
}



module_init(aesd_init_module);
module_exit(aesd_cleanup_module);
