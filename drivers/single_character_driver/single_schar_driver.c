#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/fs.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/uaccess.h>
#include <linux/cdev.h>
#include <linux/kdev_t.h>

#define MAX_MINOR 1

struct cdev *cdev_variable;
dev_t devvt;
char *ker_buff;
struct class *driver_class;
struct device *module_device ;
size_t total_data_size ; 

static ssize_t custom_reader(struct file *file , char  __user *buffer , size_t size , loff_t *offset){

	if(*offset >= total_data_size){
		pr_info("EOF REACHED \n");
		return 0;
	};

	if(total_data_size == 0 || !ker_buff){
		pr_err("NOTHING TO READ \n");
		return 0;
	};

	size_t data_left =  total_data_size - *offset ;
	size_t bytes_to_read = (size < data_left) ? size : data_left ;

	if(copy_to_user(buffer,ker_buff,bytes_to_read)){
		pr_err("COPYING ERROR \n");
		return -EFAULT;
	};

	*offset += bytes_to_read;

	return bytes_to_read ;

};

static ssize_t custom_writer(struct file *file , const char __user *buffer , size_t size , loff_t *offset){
	total_data_size += size ;
        ker_buff = krealloc(ker_buff , total_data_size ,GFP_KERNEL);

	if(!ker_buff){
		return -ENOMEM;
	};

	if(copy_from_user(ker_buff,buffer,size)){
	        return -EFAULT;
	 };

	 pr_info("DATA HAS NOW BEEN SAVED IN THE KERNEL BUFFER \n");

	 return size ;
};

static int custom_opener(struct inode *inode , struct file *file){
	pr_info("NOW IN THE CUSTOM OPEN \n");
	char *buff;

	buff =  kmalloc(PATH_MAX , GFP_KERNEL);
	if(!buff){
		return -ENOMEM;
	};

	char *path_string;
	path_string = d_path(&file->f_path,buff,PATH_MAX);
	if(IS_ERR(path_string)){
			pr_err("error in printing the path \n");
	}else {
		pr_info("file path : %s ", path_string);
	};

	int major , minor;
	minor = MINOR(devvt);
	major = MAJOR(devvt);
	pr_info("MAJOR : %d | MINOR : %d \n" , major , minor);

	return 0;
};

static int purger(struct inode *inode ,  struct file *file){
	pr_info("EXITNG THE PROCESS ..\n");

	pr_info("custom close : the file has been closed by the kernel  \n");

	return 0;
};
static int dispatcher(void){
	pr_info("EXITNG THE PROCESS ..\n");

	device_destroy(driver_class,devvt);
	pr_info("device destroyed \n");
	class_destroy(driver_class);
	pr_info("class destroyed \n");
	unregister_chrdev_region(devvt,MAX_MINOR);
	pr_info("DRV_T UNREGISTERED \n");
	return 0;
};

struct file_operations fops = {
	.owner = THIS_MODULE , 
	.open  = custom_opener,
	.write = custom_writer,
	.read  = custom_reader,
	.release = purger,
};

static int initilizer(void){
	pr_info("STARTING THE INILIZATION PHASE \n");

//-------------------------------------------------------------------------------------------------//
        int res ;
	res = alloc_chrdev_region(&devvt,0,MAX_MINOR,"single_char_driver");
	if(res <0){
		pr_err("DEVT ALLOCTIOIN ERROR  \n");
		return 0;
	};
	pr_info("OPERATION COMPLETED \n");
//--------------------------------------------------------------------------------------------------//

	int major , minor ;
	major =  MAJOR(devvt);
	minor =  MINOR(devvt);

//--------------------------------------------------------------------------------------------------//

	cdev_variable = cdev_alloc();
	cdev_variable->ops = &fops ; 
	cdev_variable->owner = THIS_MODULE ;
	if(cdev_add(cdev_variable,devvt,1)<0){
		pr_err("CDEV ERROR \n");
		goto fail_cdev;
	};

//-------------------------------------------------------------------------------------------------//

	driver_class = class_create("single_char_driver");
	if(IS_ERR(driver_class)){
		pr_err("CLASS CREATION ERROR (error code : %ld) \n ",PTR_ERR(driver_class));
		goto fail_class;
	};

	module_device = device_create(driver_class,NULL,devvt,NULL,"sin_char_driver");
	if(IS_ERR(module_device)){
		pr_err("MODULE DEVICE CREATION ERROR (error code : %ld", PTR_ERR(module_device));
		goto fail_device ;
	};
	pr_info("OPERATION SUCCESSFUL : DEVICE FILE CREATED (major: %d | minor : %d) " ,major , minor);//-------------------------------------------------------------------------------------------------//


	return 0;


fail_device :
	pr_info("DESTROYING THE DEVICE  \n");
	device_destroy(driver_class,devvt);
	pr_info("DEVICE DESTROY \n");

fail_class :
	pr_info("DESTROYING THE CLASS \n");
	class_destroy(driver_class);
	pr_info("THE CLASS HAS BEEN DESTROYED \n");


fail_cdev :
	pr_info("UNREGISTERING THE DEV_T ASSIGNED \n");
	unregister_chrdev_region(devvt , MAX_MINOR);
	pr_info("DEV_T UNREGISTERED \n");

	return -1;
};

static int __init my_init(void){
	pr_info("NOW IN THE INIT FUNCTION \n");
	int res ;

	res =  initilizer();
	if(res<0){
		pr_err("INITILASATION ERROR \n");
		return -1;
	};

	pr_info("OPERATIN COMPLETED\n");
	return 0;
};

static void __exit my_exit(void){
	pr_info("NOW EXITING FROM THE DRIVER \n");
	if(dispatcher()<0){
		pr_err("OPERATION FAILED \n");
	};

	pr_info("OPERTION COMPLETE \n");
};


module_init(my_init);
module_exit(my_exit);


MODULE_LICENSE("GPL");
MODULE_AUTHOR("BLACKSTAR");
MODULE_DESCRIPTION("a simple character driver");

