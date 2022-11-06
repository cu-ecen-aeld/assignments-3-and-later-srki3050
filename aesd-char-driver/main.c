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
    filp->private_data = dev;
    return 0;
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
    int lockstatus;
    long bytes_failed_to_copy;
    PDEBUG("read %zu bytes with offset %lld",count,*f_pos);
    lockstatus = mutex_lock_interruptible(&aesd_device.lock);
    if(!lockstatus){
    	current_entry = aesd_circular_buffer_find_entry_offset_for_fpos(&dev->cbuf_entity, *f_pos, &offset);
    	bytes_read = current_entry -> size - offset;
    	if(bytes_read > count){
    		bytes_read = count;
    	}
    }	
    bytes_failed_to_copy = copy_to_user(buf, (current_entry->buffptr + offset), bytes_read);
    if(!bytes_failed_to_copy){
    	retval = bytes_read;
    	*f_pos += retval;
    }
    mutex_unlock(&aesd_device.lock);
    return retval;
}

ssize_t aesd_write(struct file *filp, const char __user *buf, size_t count,
                loff_t *f_pos)
{
    ssize_t retval = -ENOMEM;
    //const char* ret_entry = NULL;
    size_t error_bytes_num;
    struct aesd_dev* device = filp->private_data;
    if(mutex_lock_interruptible(&device->lock))
	return -ERESTARTSYS;
    if(device->buffer_entity.size == 0){
  	device->buffer_entity.buffptr = kzalloc(count, GFP_KERNEL);
    } 
    else{
  	device->buffer_entity.buffptr = krealloc(device->buffer_entity.buffptr, device->buffer_entity.size + count, GFP_KERNEL);
    }

if(device->buffer_entity.buffptr == NULL)
{
  retval = -ENOMEM;
  goto out;
}  

retval = count;

error_bytes_num = copy_from_user((void*)(&device->buffer_entity.buffptr[device->buffer_entity.size]), buf, count);

if(error_bytes_num)
{
  retval = retval - error_bytes_num;
}

device->buffer_entity.size += retval;


if(strchr((char*)(device->buffer_entity.buffptr), '\n'))
{
  aesd_circular_buffer_add_entry(&device->cbuf_entity, &device->buffer_entity);
  device->buffer_entity.buffptr = NULL;
  device->buffer_entity.size = 0;
}
out:
mutex_unlock(&device->lock);
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
