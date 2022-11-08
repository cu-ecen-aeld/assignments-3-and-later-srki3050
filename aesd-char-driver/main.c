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
 * Source:	https://kernel.org/doc/htmldocs/kernel-api/API---copy-to-user.html
 *
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/printk.h>
#include <linux/types.h>
#include <linux/cdev.h>
#include <linux/mutex.h>
#include <linux/fs.h> // file_operations
#include <linux/slab.h> // for kmalloc and kfree
#include "aesdchar.h"
#include "aesd-circular-buffer.h"
int aesd_major =   0; // use dynamic major
int aesd_minor =   0;

MODULE_AUTHOR("Sricharan Kidambi"); /** TODO: fill in your name **/
MODULE_LICENSE("Dual BSD/GPL");

struct aesd_dev aesd_device;
// Perform Scull driver implementation - Source, Linux Device Drivers Chapter 3
int aesd_open(struct inode *inode, struct file *filp)
{
    struct aesd_dev *dev = NULL;
    PDEBUG("open");
    dev = container_of(inode->i_cdev, struct aesd_dev,cdev);
    filp->private_data = dev;	/* for other methods */
    return 0;			/* Success */
}
// aesd_release - Deallocate Data 
int aesd_release(struct inode *inode, struct file *filp)
{
    PDEBUG("release");
    return 0;
}

ssize_t aesd_read(struct file *filp, char __user *buf, size_t count,
                loff_t *f_pos)
{
    //Source: https://ufal.mff.cuni.cz/~jernej/2018/docs/predavanja03.pdf
    struct aesd_dev *dev = (struct aesd_dev *)filp->private_data;	//filp pointer
    struct aesd_buffer_entry *current_entry = NULL;
    ssize_t retval = 0;
    ssize_t bytes_read = 0;
    ssize_t offset = 0;
    size_t bytes_failed_to_copy;
    int lockstatus;
    PDEBUG("read %zu bytes with offset %lld",count,*f_pos);
    lockstatus = mutex_lock_interruptible(&dev->lock);
    if(!lockstatus){
    	current_entry = aesd_circular_buffer_find_entry_offset_for_fpos(&dev->cbuf_entity, *f_pos, &offset);
    	if(current_entry == NULL){
    		mutex_unlock(&dev->lock);
    		return 0;
    	}
    	bytes_read = current_entry->size - offset;
    	bytes_read = bytes_read > count?count:bytes_read;
    }
    else{
    	return -ERESTARTSYS;
    }
    bytes_failed_to_copy = copy_to_user(buf, (current_entry->buffptr + offset), bytes_read);
    if(bytes_failed_to_copy){
    	mutex_unlock(&dev->lock);
    	return -EFAULT;
    }
    else{
    	*f_pos += bytes_read;
    	retval = bytes_read;
    }
    mutex_unlock(&dev->lock);
    return retval;
}

ssize_t aesd_write(struct file *filp, const char __user *buf, size_t count,
                loff_t *f_pos)
{
    ssize_t retval = 0;
    const char * ret_entry = NULL;
    size_t num_bytes = 0;
    struct aesd_dev *dev = NULL;
    PDEBUG("aesd_write begin");
    PDEBUG("write %zu bytes with offset %lld",count,*f_pos);
    /**
     * TODO: handle write
     */
    dev = filp->private_data;
    if (!dev) {
        PDEBUG("aesd_write: dev == NULL");
        return -ENOMEM;
    }

    if (mutex_lock_interruptible(&dev->lock)) {
        PDEBUG("aesd_write: mutex_lock_interruptible");
        return -ERESTARTSYS;
    }

    dev->buffer_entity.buffptr = (dev->buffer_entity.size == 0) ? kzalloc(count, GFP_KERNEL) :  krealloc(dev->buffer_entity.buffptr, dev->buffer_entity.size + count, GFP_KERNEL);

    if (!dev->buffer_entity.buffptr) {
        mutex_unlock(&dev->lock);
        PDEBUG("aesd_write: mutex_unlock");
        return -ENOMEM;
    }

    num_bytes = copy_from_user((void *)(&dev->buffer_entity.buffptr[dev->buffer_entity.size]), buf, count);
    retval = num_bytes ? count - num_bytes : count;

    dev->buffer_entity.size += retval;
   
    if (strchr((char*)(dev->buffer_entity.buffptr), '\n')) {
        ret_entry = aesd_circular_buffer_add_entry(&dev->cbuf_entity, &dev->buffer_entity);
        if (ret_entry) {
            kfree(ret_entry);
        }
        dev->buffer_entity.size = 0;
        dev->buffer_entity.buffptr = NULL;
    }
    mutex_unlock(&dev->lock);
    PDEBUG("aesd_write end");
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

    mutex_init(&aesd_device.lock);

    result = aesd_setup_cdev(&aesd_device);

    if( result ) {
        unregister_chrdev_region(dev, 1);
    }
    return result;

}

void aesd_cleanup_module(void)
{
    struct aesd_buffer_entry *current_entry = NULL;
    int index = 0;
    dev_t devno = MKDEV(aesd_major, aesd_minor);

    cdev_del(&aesd_device.cdev);

    //Deallocate every allocated memory
    AESD_CIRCULAR_BUFFER_FOREACH(current_entry, &aesd_device.cbuf_entity, index){
    	if(current_entry->buffptr != NULL){
    		kfree(current_entry->buffptr);
    	}
    }
    unregister_chrdev_region(devno, 1);
}



module_init(aesd_init_module);
module_exit(aesd_cleanup_module);
