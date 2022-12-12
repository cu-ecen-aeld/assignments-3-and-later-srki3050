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
#include <linux/cdev.h> //To use cdev functions and file operations
#include <linux/fs.h> // file_operations used for alloc_chrdev_region and unregister_chrdev_region
#include <linux/uaccess.h> //for copy to user and copy from user
#include <linux/slab.h>
#include <linux/kdev_t.h>
#include "aesdchar.h"
#include "aesd-circular-buffer.h"
int aesd_major =   0;
int aesd_minor =   0;
//Module description required for any module
MODULE_AUTHOR("Your Name Here"); /** TODO: fill in your name **/
MODULE_LICENSE("Dual BSD/GPL");

struct aesd_dev aesd_device;
//Source Linux Device Drivers chapter 3
//opens a new file object and linking it to the corresponding object
//inode contains general information about a file.
//file - tracks interaction on an open file by the user processes.
//@parameters  - Pointer of inode associated with the file name
//		- Pointer to file   
int aesd_open(struct inode *inode, struct file *filp)
{
    struct aesd_dev *dev;
    dev=container_of(inode->i_cdev, struct aesd_dev, cdev);
    filp->private_data=dev;
    PDEBUG("open");
    return 0;
}

int aesd_release(struct inode *inode, struct file *filp)
{
    PDEBUG("release");
    return 0;
}
//Function:	aesd_read
//Purpose:	read from the hardware and submit data back to the user.
//@parameters:	filep 	- file pointer,
//		count 	- size of data transfer,
//		buf 	- empty buffer where newly read data has to be placed
//		f_pos 	- long offset type indicating the file position that user is accessing
//@returns:	signed size type
ssize_t aesd_read(struct file *filp, char __user *buf, size_t count,
                loff_t *f_pos)
{
	ssize_t retval = 0;
	struct aesd_dev *dev = (struct aesd_dev *)filp->private_data;
    	struct aesd_buffer_entry *pos = NULL;
    	ssize_t read_bytes = 0;
    	ssize_t buffer_entry_offset = 0;
    	PDEBUG("read %zu bytes with offset %lld",count,*f_pos);
    	mutex_lock_interruptible(&aesd_device.lock);
    	/****************************** Perform Read Function ***********************************/
    	pos = aesd_circular_buffer_find_entry_offset_for_fpos(&dev->buffer, *f_pos, &buffer_entry_offset);
    	read_bytes = pos->size - buffer_entry_offset;
    	read_bytes = read_bytes > count?count:read_bytes;
    	copy_to_user(buf, (pos->buffptr + buffer_entry_offset), read_bytes);
    	retval = read_bytes;
    	*f_pos += retval;
    	/****************************** Read function Complete **********************************/
    	mutex_unlock(&aesd_device.lock);
	return retval;
}
//Function:	aesd_write
//Purpose:	Accept data from the user space and write the data to the hardware
//@parameters:	filp	- file pointer
//		buf	- value to write in kernel
//		count	- how many bytes you are trying to count
//		f_pos 	- long offset type indicating the file position that user is accessing
//@returns:	signed size type
ssize_t aesd_write(struct file *filp, const char __user *buf, size_t count,
                loff_t *f_pos)
{
	ssize_t retval = -ENOMEM;
	int i;
	struct aesd_dev *dev = (struct aesd_dev *)filp->private_data;
	PDEBUG("write %zu bytes with offset %lld",count,*f_pos);
    	int interuptible_lock = mutex_lock_interruptible(&aesd_device.lock);
    	/********************************************Accessing data and perform write function****************************************/
    	//if there are no elements in the circular buffer
    	if(!dev->entry.size) {
    		//allocate the necessary amount of memory in the kernel for the required bytes you are writing
    		dev->entry.buffptr = kmalloc((sizeof(char)*count),GFP_KERNEL);
    		//set it to null
    		memset(dev->entry.buffptr, 0, sizeof(char)*count);
    	}	
    	else{
    		dev->entry.buffptr = krealloc(dev->entry.buffptr, (dev->entry.size + count)*sizeof(char), GFP_KERNEL);
    	}
    	int total_bytes_copied = copy_from_user((void *)(&dev->entry.buffptr[dev->entry.size]), buf, count);
    	retval = count;
    	dev->entry.size += count;
    	for (i=0; i<dev->entry.size; i++)
    	{
    		if(dev->entry.buffptr[i] == '\n')
    		{
    			aesd_circular_buffer_add_entry(&dev->buffer, &dev->entry);
    			dev->entry.buffptr = NULL;
    			dev->entry.size = 0;
    		}
    	}
    	/******************************************Write complete, release the resources**********************************************/
    	mutex_unlock(&aesd_device.lock);
    	*f_pos = 0;
    	return retval;
}
//THIS_MODULE - used to prevent the module from being unloaded while the structure is still in use
//Macro to the module variable that points to the current module.
struct file_operations aesd_fops = {
    .owner =    THIS_MODULE,
    .read =     aesd_read,
    .write =    aesd_write,
    .open =     aesd_open,
    .release =  aesd_release,
};

static int aesd_setup_cdev(struct aesd_dev *dev)
{
    //if you have a major and minor number, you can convert it to dev_t
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
    //typedef of uint32_t used to represent the device number
    //out of 32bits, 12 bits for major number and 20 bits for minor number
    dev_t dev = 0;
    int result;
    //register your device into the linux kernel - do this to register during module initialize to the kernel.
    //Major number is dynamically allocated by the kernel
    //aesdchar is the name we give to identify the device number range
    //return type is int.
    result = alloc_chrdev_region(&dev, aesd_minor, 1, "aesdchar");
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
    PDEBUG("\r\nMODULE LOADED SUCCESSFULLY");
    return result;

}

void aesd_cleanup_module(void)
{
    uint8_t index = 0;
    struct aesd_buffer_entry *entry = NULL;
    //Create a dev_t
    dev_t devno = MKDEV(aesd_major, aesd_minor);
    //To remove a char device from the system
    cdev_del(&aesd_device.cdev);
    
    AESD_CIRCULAR_BUFFER_FOREACH(entry,&aesd_device.buffer,index) {
    	if(entry->buffptr != NULL){
  		kfree(entry->buffptr);
  	}
    }
    mutex_destroy(&aesd_device.lock);
    unregister_chrdev_region(devno, 1);
}



module_init(aesd_init_module);
module_exit(aesd_cleanup_module);
