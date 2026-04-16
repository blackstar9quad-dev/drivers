#include <linux/fs.h>
#include <linux/kdev_t.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/types.h>

#define MAX_MINOR 4

struct my_data{
	struct cdev cdev_pointer;
	struct file_operations *file_op;
	struct device *module_devices[MAX_MINOR];
	dev_t devt;
	struct class *driver_class;
	char *data;
	size_t total_data_size;
};
static struct my_data global_variable;

static ssize_t custom_writer(struct file *file , const  char __user *buffer , size_t size , loff_t *off){

	struct my_data *data_struct_pt = file->private_data ; 

	int minor , major;
	minor = MINOR(data_struct_pt->devt);
	major = MAJOR(data_struct_pt->devt);
	size_t data_size ; 
	data_size = data_struct_pt->total_data_size + size ; 
	data_struct_pt->data = krealloc(data_struct_pt->data ,data_size, GFP_KERNEL);

	if(!data_struct_pt->data){
		return -ENOMEM;
	};

	copy_from_user(data_struct_pt->data + size,buffer,size);

	pr_info("MESSAGE WRITTEN TO DEVICE STRUCT for MAJOR : %d | MINOR : %d \n",major,minor);

	return size;
};

static ssize_t custom_reader(struct file *file , const char __user *buffer , size_t size , loff_t *offset){


	struct my_data *data_struct_pt = file->private_data;

	int minor , major ;
	minor = MINOR(data_struct_pt->devt);
	major = MAJOR(data_struct_pt->devt);

	if(!data_struct_pt->data || data_struct_pt->total_data_size == 0){
		pr_info("NO DATA TO READ (data:%ld)\n",data_struct_pt->total_data_size);
		return 0;
	};

	if(*offset >= data_struct_pt->total_data_size){
		return 0;
	};

	size_t remaining = data_struct_pt->total_data_size - *offset ;
	size_t bytes_to_read = (size < remaining) ? size : remaining ;

	if(copy_to_user(buffer , data_struct_pt->data + *offset , bytes_to_read)){
		return -EFAULT;
	};

	*offset += bytes_to_read ;

	return bytes_to_read ;

};

static int custom_opener(struct inode *inode , struct file *file){
	pr_info("NOW IN THE CUSTOM OPENER \n");

	struct my_data *data = container_of(inode->i_cdev, struct my_data , cdev_pointer); //get's the pointer the start of the global_variable hence the struct driver_data 

	struct my_data file->private_data = data;

	int minor , major ; 
	minor =  MINOR(file->private_data->devt);
	major =  MAJOR(file->private_data->devt);

	pr_info("DEVICE DEV_T : %d",file->private_data->devt);
	pr_info("MINOR : %d | MAJOR : %d \n",minor,major);

	return 0;
};

static int initiliser(){
	int res_inode ,  ret;
//--------------------------------------------------------------------------------------------------//                  	//dev_t (major:minor) assignment phase
	printk(KERN_INFO "ALLOCATING THE DEV_t VALUE FOR THE FILE \n");
	res_inode = alloc_chrdev_region(&global_variable.devt,0,MAX_MINOR,"custom_driver");
	if(res_inode < 0){
		pr_err("ERROR IN MAKING THE MAJOR:MINOR FOR THE SPACE (error code : %d)\n",res_inode);
		return res_inode ;
	};

	printk(KERN_INFO "OPERATION COMPLETE \n");
//-------------------------------------------------------------------------------------------------//
 
//-------------------------------------------------------------------------------------------------//
                        //cdev initilisation phase
	printk(KERN_INFO "CREATING THE CDEV \n");
	cdev_init(&global_variable.cdev_pointer,&file_ops);
	global_variable.cdev_pointer.owner = THIS_MODULE ;
	ret = cdev_add(global_variable.cdev_pointer,global_variable.devt,MAX_MINOR);
	if(ret<0){
		goto fail_cdev;
	};
	printk("OPERATION COMPLETE\n");
//-------------------------------------------------------------------------------------------------//

//-------------------------------------------------------------------------------------------------//
                         //class and device file creation 
	printk(KERN_INFO "CREATING THE CLASS AND DEVICE FILES\n");
	global_variable.driver_class = class_create("character_driver_custom");
	if(IS_ERR(global_variable.driver_class)){
		pr_err("ERROR IN CREATE THE DRIVER CLASS (error code : %d) \n",PTR_ERR(global_variable.driver_class));
		goto fail_class;
	};
	for(int i =0 ; i<MAX_MINOR ; i++){
		global_variable.module_devices[i] = device_create(&global_variable.driver_class,NULL,global_variable.devt,NULL,"custom_driver");
		if(IS_ERR(global_variable.module_devices[i])){
			pr_err("ERROR IN CREATING THE MINOR DEVICE FILE (error code :%d) \n");
			goto fail_devices;
		};
	};

	printk(KERN_INFO "OPERATION COMPLETE \n");

//-------------------------------------------------------------------------------------------------//

//-------------------------------------------------------------------------------------------------//
                                      //FAIL SAFE
fail_class :
	class_destroy(global_variable.driver_class);
	printk(KERN_INFO "class destroyed \n");

fail_devices :
	for(int i=0 ; i<MAX_MINOR ; i++){
		device_destroy(global_variable.driver_class , global_variable.devt);
		printk(KERN_INFO "DEVICE %d has been destroyed \n",i);
	};
	class_destroy(global_variable.driver_class);
	pr_info("CLASS DESTROYED \n");

fail_cdev :
	unregister_chrdev_region(global_variable.devt , MAX_MINOR);
	pr_info("DEVT UNREGISTERED \n");
	return ret;
//-------------------------------------------------------------------------------------------------//
	return 0;
};



struct file_operations f_ops = {
	.owner = THIS_MODULE,
	.write = custom_writer,
	.read  = custom_reader, 
	.open  = custom_opener,
	.release = custom_releaser,
};



static int __init my_driver_init(){
	int result;

	result = initiliser();
	if(result<0){
		printk(KERN_INFO "INILIZATION FAILED ERROR IN FUNCTION CALL\n");
		return -1;
	};

	return 0;

};

static void __exit my_driver_exit(){

	pr_info("EXITING FROM THE DRIVER \n");

	for(int i=0 ; i<MAX_MINOR ; i++){
		device_destroy(global_variable.driver_class,global_variable.devt);
		pr_info("device %d destroyed \n",i);
	};

	class_destroy(global_variable.driver_class);
	pr_info("CLASS DESTROYED \n");

	unregister_chrdev_region(global_variable.devt , MAX_MINOR);
	pr_info("MAJOR:MINOR DELETED AND DESTROYED FROM SYS/DEVICES \n");
};

module_init(my_driver_init);
module_exit(my_driver_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("BLACKSTAR");
MODULE_DESCRIPTION("A SIMPLE CHARACTER DRIVER , FIRST CUSTOM DRIVER CREATED BY BLACKSTAR");

