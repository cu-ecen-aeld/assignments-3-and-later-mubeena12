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

/**
 * References:
 *   1. ChatGPT: Example of char device kernel driver implementation in C
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/printk.h>
#include <linux/types.h>
#include <linux/cdev.h>
#include <linux/fs.h> // file_operations
#include <linux/slab.h>
#include "aesdchar.h"
#include "aesd_ioctl.h"

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
    filp->private_data = NULL;
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
            PDEBUG("ERROR: copy_to_user failed!");
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

    const char *replaced_buffer = NULL;
    char *temp_buf;
    size_t temp_buf_size = count;
    size_t temp_buf_write_offset = 0;

    size_t bytes_not_copied = 0;
    struct aesd_buffer_entry new_entry;
    PDEBUG("write %zu bytes with offset %lld",count,*f_pos);
    /**
     * TODO: handle write
     */

    if (mutex_lock_interruptible(&dev->write_mutex)) {
        retval = -ERESTARTSYS;
        goto exit;
    }

    if (count == 0) {
        retval = 0;
        goto exit; // Nothing to write
    }

    if (dev->cached_entry.buffptr != NULL) {
        temp_buf_write_offset = dev->cached_entry.size;
        temp_buf = (char *)dev->cached_entry.buffptr;
        temp_buf_size += temp_buf_write_offset;

        temp_buf = krealloc(temp_buf, temp_buf_size, GFP_KERNEL);
    }
    else {
        temp_buf = kmalloc(temp_buf_size, GFP_KERNEL); // Allocate memory for the buffer
    }
    if (!temp_buf) {
        retval = -ENOMEM; // Memory allocation failed
        goto exit; 
    }

    bytes_not_copied = copy_from_user(&temp_buf[temp_buf_write_offset], buf, count);

    if (bytes_not_copied == 0) {
        retval = count;
    }
    else {
        retval = count - bytes_not_copied;
        temp_buf_size -= bytes_not_copied;
        PDEBUG("WARNING: copy_from_user failed to copy %zu bytes (total expected = %zu bytes).\n", bytes_not_copied, count);
    }

    if (memchr(temp_buf, '\n', temp_buf_size) != NULL) {
        new_entry.buffptr = temp_buf;
        new_entry.size = temp_buf_size;
        replaced_buffer = aesd_circular_buffer_add_entry(&dev->circular_buffer, &new_entry);

        if (replaced_buffer != NULL)
        {
            kfree(replaced_buffer);
        }

        dev->cached_entry.buffptr = NULL;
        dev->cached_entry.size= 0;
    }
    else {
        dev->cached_entry.buffptr = temp_buf;
        dev->cached_entry.size = temp_buf_size;
    }

exit:
    mutex_unlock(&dev->write_mutex);
    return retval;
}

loff_t aesd_llseek(struct file *filp, loff_t off, int whence)
{
    struct aesd_dev *dev = filp->private_data;
    loff_t total_entries_size =  aesd_circular_buffer_entries_total_size(&dev->circular_buffer);

    // Use fixed_size_llseek to handle the seek operation
    return fixed_size_llseek(filp, off, whence, total_entries_size);
}

static long aesd_adjust_file_offset(struct file *filp, unsigned int write_cmd, unsigned int write_cmd_offset)
{
    long retval = 0;
    struct aesd_dev *dev = filp->private_data;
    long buffer_start_offset = 0;
    int i;

    if ((write_cmd >= AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED) || 
        (dev->circular_buffer.entry[write_cmd].size == 0) || 
        (write_cmd_offset >= dev->circular_buffer.entry[write_cmd].size)) {
        retval = -EINVAL;
        goto exit;
    }

    for (i = 0; i < write_cmd; i++) {
        buffer_start_offset += dev->circular_buffer.entry[i].size;
    }
    filp->f_pos = buffer_start_offset + write_cmd_offset;

exit:
    return retval;
}

long aesd_unlocked_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
    long retval = -ENOTTY;
    struct aesd_seekto seek_params;

    if ((_IOC_TYPE(cmd) != AESD_IOC_MAGIC) || (_IOC_NR(cmd) > AESDCHAR_IOC_MAXNR)) {
        goto exit;
    }

    switch (cmd) {
    case AESDCHAR_IOCSEEKTO:
        if (copy_from_user(&seek_params, (struct aesd_seekto __user *)arg, sizeof(struct aesd_seekto))) {
            retval = -EFAULT;
        }
        else {
            retval = aesd_adjust_file_offset(filp, seek_params.write_cmd, seek_params.write_cmd_offset);
        }
        break;

    default:
        break;
    }

exit:
    return retval;
}

struct file_operations aesd_fops = {
    .owner =    THIS_MODULE,
    .read =     aesd_read,
    .write =    aesd_write,
    .open =     aesd_open,
    .release =  aesd_release,
    .llseek =   aesd_llseek,
    .unlocked_ioctl = aesd_unlocked_ioctl,
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
    aesd_device.cached_entry.buffptr = NULL;
    aesd_device.cached_entry.size = 0;
    mutex_init(&aesd_device.write_mutex);
    
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

    mutex_destroy(&aesd_device.write_mutex);

    unregister_chrdev_region(devno, 1);
}

module_init(aesd_init_module);
module_exit(aesd_cleanup_module);
