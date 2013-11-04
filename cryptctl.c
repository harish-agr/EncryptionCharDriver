/* OS416: Assignment 2 - Char() Device using ioctl() 
 * By: Zac Brown, Pintu Patel, Priya
 * November 3, 2013
 */



#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/errno.h>
#include <asm/uaccess.h>

#include <linux/slab.h>
#include <linux/types.h>
#include <linux/mutex.h>
#include <linux/kfifo.h>

#include "cryptctl.h"

struct scull_qset{
    void **data;
    struct scull_qset *next;
};

/*this is my scull_struct*/
typedef struct scull_dev {
    struct scull_qset *data; /*pointer to first quantum set*/
    struct scull_dev *next;    /*pointer to the next listitem*/
    int quantum;            /*the current quantum size*/
    int qset;               /*the current array size*/
    unsigned long size;      /*amount of data stored here*/
    struct semaphore sem;    /*for mutual exclusion*/
    struct cdev cdev;        /*Char Device Structure*/
}Scull_Dev;

static dev_t dev;
static struct cdev c_dev;
static struct class *cl;

int key_size=0;
char *key;

#define FIRST_MINOR 0
#define MINOR_CNT 1

#ifndef SCULL_QUANTUM
#define SCULL_QUANTUM 4000
#endif

#ifndef SCULL_QSET
#define SCULL_QSET    1000
#endif

static struct class* hello_class = NULL;
static struct device* hello_device = NULL;
static struct device* dhello_device = NULL;

int scull_quantum=4000;
int scull_qset=1000;

/*start with major number = 0 (it will change in init()*/
static int simple_major = 0;
static int dsimple_major = 0;

/*start with minor number = 0 (it will increment with each creation) */
int scull_minor=0;



/*we have a static array of cdevs*/
static Scull_Dev HelloDevs[6];
static Scull_Dev dHelloDevs[6];

int scull_trim(Scull_Dev *dev)
{
    
    Scull_Dev *next,*dptr;
    int qset = dev->qset; /*dev is not null*/
    int i;
    printk(KERN_ALERT "Inside scull_trim\n");
    /*trying to free data->data[i]*/
    printk(KERN_ALERT "Trying to free dptr->data->data[i]\n");
    for(dptr=dev; dptr; dptr=next){ /*for all list items*/
	if(dptr->data){
	    for(i=0;i<qset;i++)
		if(dptr->data->data[i])
		    kfree(dptr->data->data[i]);
	    kfree(dptr->data);
	    dptr->data=NULL;
        }
	next = dptr->next;
	kfree(dptr);
    }
    
    dev->size=0;
    dev->quantum=scull_quantum;
    dev->qset=scull_qset;
    dev->data=NULL;
    printk(KERN_ALERT "Finished scull_trim\n");
    return 0;
}

static int hello_open(struct inode *inode, struct file *filp){
    
    Scull_Dev *dev; /* device information */
    printk(KERN_ALERT "Inside hello_open()\n");
    /*  Find the device */
    dev = container_of(inode->i_cdev, Scull_Dev, cdev);
    printk(KERN_ALERT "got dev from container_of()\n");
    /* now trim to 0 the length of the device if open was write-only */
    if ( (filp->f_flags & O_ACCMODE) == O_WRONLY) {
	if (down_interruptible (&dev->sem))
		return -ERESTARTSYS;
	printk(KERN_ALERT "Entering scull_trim from hello_open()\n");
	//scull_trim(dev); /* ignore errors */
	up (&dev->sem);
    }

    /* and use filp->private_data to point to the device data */
    filp->private_data = dev;

    printk(KERN_ALERT "Device opened\n");
    printk(KERN_ALERT "Finished hello_open\n");
    return 0;
}

static int hello_release(struct inode *inod,struct file *filp){
    printk(KERN_ALERT "Device closed\n");
    return 0;
}

/*
 *Helper Function for read/write 
 */
Scull_Dev *scull_follow(Scull_Dev *dev, int n){
    printk(KERN_ALERT "Inside scull_follow\n");
    while (n--) {
	if (!dev->next) {
	    dev->next = kmalloc(sizeof(Scull_Dev), GFP_KERNEL);
	    memset(dev->next, 0, sizeof(Scull_Dev));
	}
	dev = dev->next;
	continue;
    }
    printk(KERN_ALERT "Finished scull_follow\n");
    return dev;
}

/*read from char driver*/
static ssize_t hello_read(struct file *filp, char __user *buf, size_t count,loff_t *f_pos){
	
	Scull_Dev *dev = filp->private_data; /* the first listitem */
	Scull_Dev *dptr;
	int quantum = dev->quantum;
	int qset = dev->qset;
	int itemsize = quantum * qset; /* how many bytes in the listitem */
	int item, s_pos, q_pos, rest;
	ssize_t retval = 0;
       
	if(key_size==0){
	    printk(KERN_ALERT "No key...\n");
	    return -2;
	}
        printk(KERN_ALERT "Inside hello_read()\n");
	printk(KERN_ALERT "count is %d\n",count);
	if (down_interruptible (&dev->sem))
		return -ERESTARTSYS;
	if (*f_pos > dev->size) 
		goto nothing;
	if (*f_pos + count > dev->size)
		count = dev->size - *f_pos;
	/* find listitem, qset index, and offset in the quantum */
	item = ((long) *f_pos) / itemsize;
	rest = ((long) *f_pos) % itemsize;
	s_pos = rest / quantum; q_pos = rest % quantum;

	printk(KERN_ALERT "Entering scull_follow from hello_read()\n");
    	/* follow the list up to the right position (defined elsewhere) */
	dptr = scull_follow(dev, item);

	if (!dptr->data->data)
		goto nothing; /* don't fill holes */
	if (!dptr->data->data[s_pos])
		goto nothing;
	if (count > quantum - q_pos)
		count = quantum - q_pos; /* read only up to the end of this quantum */

	printk(KERN_ALERT "Calling copy_to_user()\n");
	if (copy_to_user (buf, dptr->data->data[s_pos]+q_pos, count)) {
		retval = -EFAULT;
		goto nothing;
	}
        
	printk(KERN_ALERT "Finished copy_to_user()\n");
	printk(KERN_ALERT "read %s from user\n",buf);
	up (&dev->sem);

	*f_pos += count;
	return count;

  nothing:
	up (&dev->sem);
	return retval;
}

/*
 * Write to encryption driver
 * Read key from cryptctl
 * Encrypt user input based on key
*/
static ssize_t hello_write (struct file *filp, const char __user *buf, size_t count,loff_t *f_pos){

	Scull_Dev *dev = filp->private_data;
	Scull_Dev *dptr;
	int quantum = dev->quantum;
	int qset = dev->qset;
	int itemsize = quantum * qset;
	int item, s_pos, q_pos, rest;
	ssize_t retval = -ENOMEM; /* our most likely error */
	int i; int j=0;
	int m,k,n;
	char *message;
	printk(KERN_ALERT "Inside hello_write()\n");
	if(key_size==0){
	    printk(KERN_ALERT "No key...\n");
	    return -2;
	}
	if (down_interruptible (&dev->sem))
		return -ERESTARTSYS;

	/* find listitem, qset index and offset in the quantum */
	item = ((long) *f_pos) / itemsize;
	rest = ((long) *f_pos) % itemsize;
	s_pos = rest / quantum; q_pos = rest % quantum;

	/* follow the list up to the right position */
	dptr = scull_follow(dev, item);
	printk(KERN_ALERT "finished scull_follow in hello_write()\n");
	if (!dptr->data) {
		printk(KERN_ALERT "calling kmalloc for dptr->data");
		dptr->data = kmalloc(sizeof(struct scull_qset),GFP_KERNEL);
		memset(dptr->data, 0, sizeof(struct scull_qset));
                printk(KERN_ALERT "calling memset for dptr->data\n");	        
		
                printk(KERN_ALERT "calling kmalloc for dptr->data->data\n");
		dptr->data->data = kmalloc(qset * sizeof(char *), GFP_KERNEL);
		printk(KERN_ALERT "calling memset for dptr->data->data\n");
		memset(dptr->data->data, 0, qset * sizeof(char *));
	}
	printk(KERN_ALERT "finished first if\n");
	/* Allocate a quantum using the memory cache */
	if (!dptr->data->data[s_pos]) {
		printk(KERN_ALERT "kmalloc for dptr->data->data[s_pos]\n");
		dptr->data->data[s_pos] = kmalloc(quantum,GFP_KERNEL);
		if (!dptr->data->data[s_pos])
			goto nomem;
		printk(KERN_ALERT "memset for dptr->data->data[s_pos]\n");
		memset(dptr->data->data[s_pos], 0, scull_quantum);
	}
	printk(KERN_ALERT "finished second if\n");
	if (count > quantum - q_pos)
		count = quantum - q_pos; /* write only up to the end of this quantum */
        	
	printk(KERN_ALERT "attempting copy_from_user()\n");
	if (copy_from_user (dptr->data->data[s_pos]+q_pos, buf, count)) {
		retval = -EFAULT;
		goto nomem;
	}
        printk(KERN_ALERT "attempting to print message\n");
	message = (char *)dptr->data->data[s_pos]+q_pos;	
	printk(KERN_ALERT "Before: %s\n",message);
	printk(KERN_ALERT "\n");
        printk(KERN_ALERT "attempting to encrypt message\n");
	/*we are writing to driver by calling write() in query_app
         *we write count bytes (count in an input argument) but
         *count also includes the null character at the end
	 *so we iterate to count-1 to avoid a messy read in the future
	 */
	
	for(i=0;i<count-1;i++){
	    if(message[i]>=97 && message[i]<=122){
	        m = message[i]-97;
	        k = key[j] - 97;
	        n = (m+k) % 26;
	        message[i]=(char)n+97;
	        j++;
	        if(j==key_size-1)
		    j=0;
	        }
	}
	printk(KERN_ALERT "After: %s\n",message);
	printk(KERN_ALERT "\n");
	*f_pos += count;
 
	printk(KERN_ALERT "finished hello_write()\n");
    	/* update the size */
	if (dev->size < *f_pos)
		dev->size = *f_pos;
	up (&dev->sem);
	return count;

  nomem:
	up (&dev->sem);
	return retval;
}

/*
 * Write to decryption driver
 * Read key from cryptctl
 * decrypt user input based on key
*/
static ssize_t decrypt_write (struct file *filp, const char __user *buf, size_t count,loff_t *f_pos){

	Scull_Dev *dev = filp->private_data;
	Scull_Dev *dptr;
	int quantum = dev->quantum;
	int qset = dev->qset;
	int itemsize = quantum * qset;
	int item, s_pos, q_pos, rest;
	ssize_t retval = -ENOMEM; /* our most likely error */
	int i; int j=0;
	int m,k,n;
	char *message;	
	printk(KERN_ALERT "Inside hello_write()\n");	
	if(key_size==0){
	    printk(KERN_ALERT "No key...\n");
	    return -2;
	}
	printk(KERN_ALERT "Inside hello_write()\n");
	
	if (down_interruptible (&dev->sem))
		return -ERESTARTSYS;

	/* find listitem, qset index and offset in the quantum */
	item = ((long) *f_pos) / itemsize;
	rest = ((long) *f_pos) % itemsize;
	s_pos = rest / quantum; q_pos = rest % quantum;

	/* follow the list up to the right position */
	dptr = scull_follow(dev, item);
	printk(KERN_ALERT "finished scull_follow in hello_write()\n");
	if (!dptr->data) {
		printk(KERN_ALERT "calling kmalloc for dptr->data");
		dptr->data = kmalloc(sizeof(struct scull_qset),GFP_KERNEL);
		memset(dptr->data, 0, sizeof(struct scull_qset));
                printk(KERN_ALERT "calling memset for dptr->data\n");	        
		
                printk(KERN_ALERT "calling kmalloc for dptr->data->data\n");
		dptr->data->data = kmalloc(qset * sizeof(char *), GFP_KERNEL);
		printk(KERN_ALERT "calling memset for dptr->data->data\n");
		memset(dptr->data->data, 0, qset * sizeof(char *));
	}
	printk(KERN_ALERT "finished first if\n");
	/* Allocate a quantum using the memory cache */
	if (!dptr->data->data[s_pos]) {
		printk(KERN_ALERT "kmalloc for dptr->data->data[s_pos]\n");
		dptr->data->data[s_pos] = kmalloc(quantum,GFP_KERNEL);
		if (!dptr->data->data[s_pos])
			goto nomem;
		printk(KERN_ALERT "memset for dptr->data->data[s_pos]\n");
		memset(dptr->data->data[s_pos], 0, scull_quantum);
	}
	printk(KERN_ALERT "finished second if\n");
	if (count > quantum - q_pos)
		count = quantum - q_pos; /* write only up to the end of this quantum */
        	
	printk(KERN_ALERT "attempting copy_from_user()\n");
	if (copy_from_user (dptr->data->data[s_pos]+q_pos, buf, count)) {
		retval = -EFAULT;
		goto nomem;
	}
        printk(KERN_ALERT "attempting to print message\n");
	message = (char *)dptr->data->data[s_pos]+q_pos;	
	for(i=0;i<count;i++){
	    printk(KERN_ALERT "%c",message[i]);
	}
	printk(KERN_ALERT "\n");
        printk(KERN_ALERT "attempting to encrypt message\n");
	/*we are writing to driver by calling write() in query_app
         *we write count bytes (count in an input argument) but
         *count also includes the null character at the end
	 *so we iterate to count-1 to avoid a messy read in the future
	 */	
	for(i=0;i<count-1;i++){
            if(message[i]>=97 && message[i]<=122){
		
	    /*get the values of letters*/
	    m = message[i]-97;
	    k = key[j] - 97;
	    n = (m-k);
            /*if the difference is negative handle the cipher appropriately*/
	    if(n<0){
		n=26+n;
	    }
            else{
	        n = n % 26;
            }
	    message[i]=(char)n+97;
	    j++;
	    if(j==key_size-1)
		j=0;
	    }
	}

	printk(KERN_ALERT "\n");
	*f_pos += count;
 
	printk(KERN_ALERT "finished hello_write()\n");
    	/* update the size */
	if (dev->size < *f_pos)
		dev->size = *f_pos;
	up (&dev->sem);
	return count;

  nomem:
	up (&dev->sem);
	return retval;
}




/*Before invoking file_ops we need to allocate and register some cdev structs
 * Scull_Dev *dev - a pointer to the device in HelloDevs[]
 * minor - the minor number we are trying to create
 * fops - the file operations associated with the device
 */
void hello_setup_cdev(Scull_Dev *dev, int minor, struct file_operations *fops, int major){
    /*create a device number*/
    int err, devno = MKDEV(major, minor);
    printk(KERN_ALERT "Inside hello_setup_cdev\n");
    /*initialize the cdev device and set the Scull_Dev params*/
    cdev_init(&dev->cdev, fops);
    dev->cdev.owner = THIS_MODULE;
    dev->cdev.ops = fops;
    dev->quantum = scull_quantum;
    dev->qset = scull_qset;
    sema_init(&dev->sem,1);
    /*add the device*/
    err = cdev_add (&dev->cdev, devno, 1);
    /* Fail gracefully if need be */
    if (err)
	printk (KERN_NOTICE "Error %d adding simple%d", err, minor);
    printk(KERN_ALERT "Finished hello_setup_cdev\n");
}

/*struct of our file operations for hello*/
static struct file_operations hello_fops = {
    .owner = THIS_MODULE,
    .open = hello_open,
    .release = hello_release,
    .read = hello_read,
    .write = hello_write,

};

/*struct of our file operations for decrypt*/
static struct file_operations dhello_fops = {
    .owner = THIS_MODULE,
    .open = hello_open,
    .release = hello_release,
    .read = hello_read,
    .write = decrypt_write,

};

/*initialize the hello device - we call hello_setup_cdev() to set up the device*/
static int hello_init(void){
    int result;
    dev_t dev,ddev;

    char name[14] = "cryptEncrypt ";
    char dname[14]= "cryptDecrypt ";
    name[12] = scull_minor+48;
    dname[12] = scull_minor+48;
    if(scull_minor>5){
	return -1;
    }
    /*create 1 hello driver*/
    if(simple_major == 0){
    result = alloc_chrdev_region(&dev, scull_minor, 1, name);
    simple_major = MAJOR(dev);
    if (result < 0) {
	printk(KERN_WARNING "simple: unable to get major %d\n", simple_major);
	return result;
    }
    }
    printk(KERN_ALERT "Hello world encrypt\n");

    printk(KERN_ALERT "trying to create hello_test%c\n",(scull_minor+65));
    /*set up the cdev struct*/
    hello_setup_cdev(&HelloDevs[scull_minor],scull_minor,&hello_fops, simple_major);
    /*create the device in /dev*/
    hello_device = device_create(hello_class, NULL, MKDEV(simple_major, scull_minor), NULL, name);
    
    /*create 1 decrypt driver*/
    if(dsimple_major == 0){
    result = alloc_chrdev_region(&ddev,scull_minor,1,dname);
    dsimple_major = MAJOR(ddev);
    if(result<0){
	printk(KERN_WARNING "simple: unable to get major %d\n", simple_major);
	return result;
    }
    }
    printk(KERN_ALERT "Hello world decrypt\n");

    printk(KERN_ALERT "trying to create dhello_test%c\n",(scull_minor+65));
    hello_setup_cdev(&dHelloDevs[scull_minor],scull_minor,&dhello_fops,dsimple_major);
    dhello_device = device_create(hello_class,NULL,MKDEV(dsimple_major,scull_minor),NULL,dname);
    
    /*there are only 4 Scull_Dev structs in HelloDevs[]*/
    if(scull_minor<=5)
        scull_minor++;
    return 0;
}


/*Right now this function deletes all hello_test drivers when invoked
 *eventually I will have it only delete the most recently created
 *similar to a Stack pop
 */
static void hello_exit(void){
    int i;
    /*we do scull_minor-1 b/c we increment that value after each creation
     *so scull_minor represents the next available device
     *but scull_minor-1 represents the last created device
     */
     if(scull_minor<=0)
	return;

     i = scull_minor -1;
	printk(KERN_ALERT "hello_test major: %d\n",simple_major);
        device_destroy(hello_class,MKDEV(simple_major,i));
	printk(KERN_ALERT "destroyed hello_test%c\n",i+65);
        cdev_del(&(&HelloDevs[i])->cdev);
	printk(KERN_ALERT "deleted hello_test%c->cdev\n",i+65);
        unregister_chrdev_region(MKDEV(simple_major, i), 1);
	printk(KERN_ALERT "unregistered hello_test%c\n",i+65);

	printk(KERN_ALERT "dhello_test major: %d\n",dsimple_major);
        device_destroy(hello_class,MKDEV(dsimple_major,i));
	printk(KERN_ALERT "destroyed dhello_test%c\n",i+65);
        cdev_del(&(&dHelloDevs[i])->cdev);
	printk(KERN_ALERT "deleted dhello_test%c->cdev\n",i+65);
        unregister_chrdev_region(MKDEV(dsimple_major, i), 1);
	printk(KERN_ALERT "unregistered dhello_test%c\n",i+65);

    if(scull_minor>0)
         scull_minor--;
    printk(KERN_ALERT "goodbye\n");
} 
static int my_open(struct inode *i, struct file *f)
{
    return 0;
}
static int my_close(struct inode *i, struct file *f)
{
    return 0;
}
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,35))
static int my_ioctl(struct inode *i, struct file *f, unsigned int cmd, unsigned long arg)
#else
static long my_ioctl(struct file *f, unsigned int cmd, unsigned long arg)
#endif
{

    switch (cmd)
    {
        case QUERY_CREATE_PAIR:
	    hello_init();
            break;
        case QUERY_DESTORY_PAIR:
	    hello_exit();
            break;
	 /*
     *I need two IOCTL calls to get/store the key
     * 1)IOCTL to get/send over the size of the key
     *        GLOBAL int key_size;
     *      copy_to/from_user(&key_size,(int*)arg,sizeof(int)) <-this gets the size over
     *  2)IOCTL to get/send the key it self over
     *        char *key;
     *        copy_to/from_user(key,(char*)arg,key_size)
     *
     */
    	case QUERY_SET_SIZE:
            /*we key_size isnt 0 AND we're in this call then its a new key so free old one*/
            if(key_size!=0){
            printk(KERN_ALERT "freeing old key\n");       
            kfree(key);
            printk(KERN_ALERT "freed old key\n");
            }
            if (copy_from_user(&key_size, (int *)arg, sizeof(int)))
            {
                return -EACCES;
            }
            printk(KERN_ALERT "key_size is %d\n",key_size);
            break;
    	case QUERY_SET_KEY:
            /*if set size fails we dont even want to try sending key*/
            if(key_size==0)
            	return -1;
            /*we need to malloc the key*/
            else{
            	printk(KERN_ALERT "malloc key\n");
            	key = (char*)kmalloc(key_size,GFP_KERNEL);
        	printk(KERN_ALERT "finished malloc\n");
             }
            if(copy_from_user(key,(char*)arg,key_size)){
        	return -EACCES;
            }
            printk(KERN_ALERT "key is %s\n",key);
            break;
    	case QUERY_GET_SIZE:
            if(copy_to_user((int*)arg,&key_size,sizeof(int))){
        	return -EACCES;
            }
            printk(KERN_ALERT "sent key size\n");
            break;
    	case QUERY_GET_KEY:
            if(key_size==0){
            	printk(KERN_ALERT "dont want to send empty key\n");
             	return -1;
            }
            if (copy_to_user((char*)arg, key, key_size))
            {
                return -EACCES;
            }
            printk(KERN_ALERT "sent %s to user\n",key);
            break;
        default:
            return -EINVAL;
    }
 
    return 0;
}
 
static struct file_operations query_fops =
{
    .owner = THIS_MODULE,
    .open = my_open,
    .release = my_close,
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,35))
    .ioctl = my_ioctl
#else
    .unlocked_ioctl = my_ioctl
#endif
};
 
static int __init query_ioctl_init(void)
{
    int ret;
    struct device *dev_ret;
    hello_class = class_create(THIS_MODULE, "hello");
 
    if ((ret = alloc_chrdev_region(&dev, FIRST_MINOR, MINOR_CNT, "cryptctl")) < 0)
    {
        return ret;
    }
 
    cdev_init(&c_dev, &query_fops);
 
    if ((ret = cdev_add(&c_dev, dev, MINOR_CNT)) < 0)
    {
        return ret;
    }
     
    if (IS_ERR(cl = class_create(THIS_MODULE, "char")))
    {
        cdev_del(&c_dev);
        unregister_chrdev_region(dev, MINOR_CNT);
        return PTR_ERR(cl);
    }
    if (IS_ERR(dev_ret = device_create(cl, NULL, dev, NULL, "cryptctl")))
    {
        class_destroy(cl);
        cdev_del(&c_dev);
        unregister_chrdev_region(dev, MINOR_CNT);
        return PTR_ERR(dev_ret);
    }
 
    return 0;
}
 
static void __exit query_ioctl_exit(void)
{
    class_unregister(hello_class);
    class_destroy(hello_class);
    
    device_destroy(cl, dev);
    class_destroy(cl);
    cdev_del(&c_dev);
    unregister_chrdev_region(dev, MINOR_CNT);
}


module_init(query_ioctl_init);
module_exit(query_ioctl_exit);




 
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Zac Brown, Pintu Patel, Priya");
MODULE_DESCRIPTION("Query ioctl() Char Driver");
