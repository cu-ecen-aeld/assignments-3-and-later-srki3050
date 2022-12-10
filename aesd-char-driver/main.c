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
#include <linux/cdev.h> //To use cdev functions and operations
#include <linux/fs.h> // file_operations
#include <linux/slab.h>
#include "aesdchar.h"
#include "aesd-circular-buffer.h"
int aesd_major =   0;
int aesd_minor =   0;

MODULE_AUTHOR("Your Name Here"); /** TODO: fill in your name **/
MODULE_LICENSE("Dual BSD/GPL");

struct aesd_dev aesd_device;
//Source Linux Device Drivers chapter 3
int aesd_open(struct inode *inode, struct file *filp)
{
    struct aesd_dev *dev = NULL;
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
//@parameters:	filep 	- file pointer,
//		count 	- size of data transfer,
//		buf 	- empty buffer where newly read data has to be placed
//		f_pos 	- long offset type indicating the file position that user is accessing
//@returns:	signed size type
ssize_t aesd_read(struct file *filp, char __user *buf, size_t count,
                loff_t *f_pos)
{
	struct aesd_dev *dev = filp->private_data;
    	ssize_t retval = 0, begin_of_the_current_string = 0,allowable_read_length = 0, bytes_kernel_is_returning_to_user_space = 0;
	//
	struct aesd_buffer_entry *read = NULL;
	PDEBUG("read %zu bytes with offset %lld",count,*f_pos);
	//if you are unable to lock the device while accessing, it might be accessed by other functions which might panic the kernel, so restart the system
	if (mutex_lock_interruptible(&dev->lock))
		return -ERESTARTSYS;
	//As described in aesd-circular-buffer.c, in case you are in the middle of a string, offset would be the start of that particular string
	//This is done so that each time a buffer is copied from kernel to user space, 
	read = aesd_circular_buffer_find_entry_offset_for_fpos(&dev->buffer, *f_pos, &begin_of_the_current_string);
	//this occurs only when the offset exceeds the total number of strings
	if(read == NULL)
	{
		PDEBUG("Error: aesd_read, data exceeding the existing list of strings\n");
		mutex_unlock(&dev->lock);
		return retval;
	}
	//if string  = "AESD_Embedded", count 5, then only AESD_ should be print, for that move the beginning to A and read till _,
	//if count > 13, still print only AESD_Embedded 
	//you are trying to read more the count paramaeter read only count, thats your maximum
	if((read->size - begin_of_the_current_string) > count)
		allowable_read_length = count;
	else
		//failing which read till wherever you want.
		allowable_read_length = (read->size - begin_of_the_current_string);
	//once you figured out what to read, copy_to_user function transfers that to the user space
	//architecture-dependent magic to ensure that data transfers between kernel and user space happen in a safe and correct way.
	bytes_kernel_is_returning_to_user_space = copy_to_user(buf, &read->buffptr[begin_of_the_current_string], allowable_read_length);
	if (allowable_read_length) {
		retval = 0;
		mutex_unlock(&dev->lock);
		return retval;
	}
	//move the current pointer to the number of bytes read
	*f_pos = *f_pos + allowable_read_length;
	retval = allowable_read_length;
	return retval;
}
//@parameters:	filp	- file pointer
//		buf	- value to write in kernel
//		count	- how many bytes you are trying to count
//		f_pos 	- long offset type indicating the file position that user is accessing
//@returns:	signed size type
ssize_t aesd_write(struct file *filp, const char __user *buf, size_t count,
                loff_t *f_pos)
{
    	struct aesd_dev *dev = filp->private_data;
	size_t copy_from_user_bytes_to_kernel;
	ssize_t retval = -ENOMEM;
	PDEBUG("write %zu bytes with offset %lld",count,*f_pos);
	if (mutex_lock_interruptible(&dev->lock))
		return -ERESTARTSYS;

	if(dev->entry.size == 0)
		dev->entry.buffptr = kzalloc(count*(sizeof(char)),GFP_KERNEL);
	else
		dev->entry.buffptr = krealloc(dev->entry.buffptr,(dev->entry.size + count), GFP_KERNEL);
	if(dev->entry.buffptr == NULL)
	{
		PDEBUG("ERROR: aesd_write unable to write to the kernel\n");
		mutex_unlock(&dev->lock);
		return retval;
	}

	copy_from_user_bytes_to_kernel = copy_from_user((void*)&dev->entry.buffptr[dev->entry.size], buf, count);
	if (copy_from_user_bytes_to_kernel) {
		retval = -EFAULT;
		mutex_unlock(&dev->lock);
		return retval;
	}
	dev->entry.size += count;
	retval = count;

	PDEBUG("Recieved data\n");
	if(strchr((char*)(dev->entry.buffptr),'\n'))
	{
		PDEBUG("Detected \n");
		if(dev->buffer.full)
		{
			kfree(dev->buffer.entry[dev->buffer.out_offs].buffptr);
		}
		aesd_circular_buffer_add_entry(&dev->buffer, &dev->entry);
		dev->entry.buffptr = NULL;
		dev->entry.size = 0;
		PDEBUG("Write complete no full\n");
	}
    	mutex_unlock(&dev->lock);
   	return count;
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
    //character device registration function to the kernel
    //Initialize an already allocated structure
    cdev_init(&dev->cdev, &aesd_fops);
    dev->cdev.owner = THIS_MODULE;
    dev->cdev.ops = &aesd_fops;
    //Once the cdev structure is set up, the final step is to tell the kernel about it
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
    //register your device into the linux kernel - do this to register during module initialize to the kernel.
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
    uint8_t index;
    struct aesd_buffer_entry *entry;
    //Create a dev_t
    dev_t devno = MKDEV(aesd_major, aesd_minor);
    //To remove a char device from the system
    cdev_del(&aesd_device.cdev);
    
    kfree(aesd_device.entry.buffptr);
    AESD_CIRCULAR_BUFFER_FOREACH(entry,&aesd_device.buffer,index) {
  	kfree(entry->buffptr);
    }
    
    unregister_chrdev_region(devno, 1);
}



module_init(aesd_init_module);
module_exit(aesd_cleanup_module);
