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
#include "aesd_ioctl.h"
int aesd_major =   0;
int aesd_minor =   0;
//Module description required for any module
MODULE_AUTHOR("srki3050"); /** TODO: fill in your name **/
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
	ssize_t buffer_entry_offset = 0;
	ssize_t bytes_to_user = 0;
	ssize_t read_bytes = 0;
	int lock_status;
	struct aesd_buffer_entry * pos = NULL;
	struct aesd_dev *dev = (struct aesd_dev *)filp->private_data;
	PDEBUG("read %zu bytes with offset %lld",count,*f_pos);
	lock_status = mutex_lock_interruptible(&aesd_device.lock);
	if (lock_status)
		return -ERESTARTSYS;
	pos = aesd_circular_buffer_find_entry_offset_for_fpos(&dev->buffer, *f_pos, &buffer_entry_offset);
	if(!pos) {
		mutex_unlock(&aesd_device.lock);
		return 0;
	}
	read_bytes = pos->size - buffer_entry_offset;
    	read_bytes = read_bytes > count?count:read_bytes;
	bytes_to_user = copy_to_user(buf, pos->buffptr + buffer_entry_offset, read_bytes);
	if (bytes_to_user) {
		mutex_unlock(&aesd_device.lock);
		return 0;
	}
	*f_pos = *f_pos + read_bytes;
	mutex_unlock(&aesd_device.lock);
	return read_bytes;
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
	int interuptible_lock, total_bytes_copied;
	int i;
	struct aesd_dev *dev = (struct aesd_dev *)filp->private_data;
	PDEBUG("write %zu bytes with offset %lld",count,*f_pos);
    	interuptible_lock = mutex_lock_interruptible(&aesd_device.lock);
    	if(interuptible_lock){
    		return -ERESTARTSYS;
    	}
    	/********************************************Accessing data and perform write function****************************************/
    	//if there are no elements in the circular buffer
    	if(!dev->entry.size) {
    		//allocate the necessary amount of memory in the kernel for the required bytes you are writing
    		dev->entry.buffptr = kmalloc((sizeof(char)*count),GFP_KERNEL);
    	}	
    	else{
    		dev->entry.buffptr = krealloc(dev->entry.buffptr, (dev->entry.size + count)*sizeof(char), GFP_KERNEL);
    	}
    	total_bytes_copied = copy_from_user((void *)(&dev->entry.buffptr[dev->entry.size]), buf, count);
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
    	return count;
}
/* Function	: aesd_ioctl
 * Purpose	: Perform the IOCTL command if the argument by calling writes data to the kernel and expecting information back.
 * Parameters	: pointer to the aesd_device, command to verify against and a pointer from which the user space is requesting data to the kernel
 * Returns	: Whether IOCTL can be performed or not, or errors like memory violation, seg faults and incorrect locking
 * References	: https://man7.org/linux/man-pages/man2/ioctl.2.html
 */
long aesd_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
    size_t bytes_copied_From_user = 0;
    int lock_status;
    loff_t pos;
    struct aesd_dev *dev = NULL;
    struct aesd_seekto seekto;
    dev = filp->private_data;
    lock_status = mutex_lock_interruptible(&dev->lock);
    if(lock_status){
    	mutex_unlock(&dev->lock);
    	return -ERESTARTSYS;
    }
    if(!dev){
    	mutex_unlock(&dev->lock);
    	return -ENOMEM;
    }	
    else if ((_IOC_TYPE(cmd) != AESD_IOC_MAGIC) || (_IOC_NR(cmd) > AESDCHAR_IOC_MAXNR) || (cmd != AESDCHAR_IOCSEEKTO)){
    	mutex_unlock(&dev->lock);
        return -ENOTTY;
    }
    if (cmd == AESDCHAR_IOCSEEKTO) {
        bytes_copied_From_user = copy_from_user(&seekto, (void *)arg, sizeof(seekto));
        if (bytes_copied_From_user)
            return -EFAULT;
        
        pos = aesd_circular_buffer_llseek(&dev->buffer, seekto.write_cmd, seekto.write_cmd_offset);
	if (pos == -EINVAL) {
		mutex_unlock(&dev->lock);
        	return -EINVAL;
    	} 
    	else {
        	filp->f_pos = pos;
        	mutex_unlock(&dev->lock);
    	}
    }
    return 0;
}
/* Function	: aesd_llseek
 * Purpose	: Perform the IOCTL command if the argument by calling writes data to the kernel and expecting information back.
 * Parameters	: pointer to the aesd_device, long value and a pointer from which the user space is requesting data to the kernel
 * Returns	: Whether IOCTL can be performed or not, or errors like memory violation, seg faults and incorrect locking
 * References	: https://man7.org/linux/man-pages/man2/llseek.2.html
 */
loff_t aesd_llseek(struct file *filp, loff_t offset, int whence)
{
    loff_t retval = 0;
    int lock_status;
    struct aesd_dev *dev = NULL;
    PDEBUG("aesd_llseek begin");
    dev = filp->private_data;
    if (!dev) {
        return -ENOMEM;
    }
    lock_status = mutex_lock_interruptible(&dev->lock);
    if (lock_status) {
        return -EINTR;
    }
    retval = fixed_size_llseek(filp, offset, whence, get_the_total_buffer_size(&dev->buffer));
    mutex_unlock(&dev->lock);
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
    .llseek =   aesd_llseek,
    .unlocked_ioctl =   aesd_ioctl,
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
