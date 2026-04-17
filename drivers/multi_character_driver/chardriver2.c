#include <linux/fs.h>
#include <linux/kdev_t.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/types.h>
#include <linux/module.h>
#include <linux/slab.h>

#define MAX_MINOR 4

struct my_data{
	struct cdev cdev;
	struct class *driver_class;
	dev_t devt;
	char *data;
	size_t total_data_size;
	struct device *module_devices;
	struct file_operations *fops;
};



static struct my_data global_variable[MAX_MINOR];

//--------------------------------------------------------------------------------------------------//
static ssize_t custom_writer(struct file *file , const char __user *buffer , size_t size , loff_t *offset){
	int minor , major ;

	struct my_data *data_struct_pt = file->private_data ;
	minor = MINOR(data_struct_pt->devt);
	major = MAJOR(data_struct_pt->devt);

	pr_info("MAJOR : %d | MINOR : %d",major,minor);
	pr_info("PATH : %pD \n",file);

	global_variable[minor].total_data_size = global_variable[minor].total_data_size + size ;
	global_variable[minor].data = krealloc(global_variable[minor].data , global_variable[minor].total_data_size , GFP_KERNEL);

	if(!data_struct_pt->data){
		return -ENOMEM;
	};
	if(copy_from_user(data_struct_pt->data,buffer,size)){
		return -EFAULT;
	};

	pr_info("MESSAGE WRITTEN TO THE STRUCT OF THE DEVICE AND NOW CAN BE READ \n");

	return size;
};
//--------------------------------------------------------------------------------------------------//

//--------------------------------------------------------------------------------------------------//
static ssize_t custom_reader(struct file *file , char __user *buffer , size_t size , loff_t *offset){
	int major , minor ;
	struct my_data *data_struct_pt = file->private_data;

	major = MAJOR(data_struct_pt->devt);
	minor = MINOR(data_struct_pt->devt);
	pr_info("MAJOR : %d | MINOR : %d",major , minor);

	if(*offset >= data_struct_pt->total_data_size){
		pr_info("EOF REACHED \n");
		return 0;
	};

	if(!data_struct_pt->data || data_struct_pt->total_data_size == 0){
		pr_info("NOTHING TO READ \n");
		return 0;
	};

	size_t data_left = global_variable[minor].total_data_size - *offset ;
	size_t bytes_to_read = (size < data_left) ? size : data_left;
	
	if(copy_to_user(buffer,data_struct_pt->data + *offset , bytes_to_read)){
			return -EFAULT ;
	};

	*offset += bytes_to_read ;

	return bytes_to_read;

};
//--------------------------------------------------------------------------------------------------//


//--------------------------------------------------------------------------------------------------//
static int custom_opener(struct inode *inode , struct file *file){
	int minor , major ;
	char *buf;
	char *path_string;

	minor = MINOR(inode->i_rdev);
	major = MAJOR(inode->i_rdev);

	buf =  kmalloc(PATH_MAX,GFP_KERNEL);

	path_string =  d_path(&file->f_path,buf,PATH_MAX);
	if(IS_ERR(path_string)){
		pr_err("ERROR IN GETTING THE PATH OF THE FILE \n");
	}else{
		pr_info("file path : %s ", path_string);
	};

	pr_info("MAJOR : %d | MINOR : %d " , major, minor);

	struct my_data *data = container_of(inode->i_cdev , struct my_data , cdev);
	file->private_data  = &global_variable[minor];

	return 0;
};
//--------------------------------------------------------------------------------------------------//

struct file_operations fops ={
	.owner = THIS_MODULE ,
	.write = custom_writer,
	.read  = custom_reader,
	.open  = custom_opener
};

static int default_state(void){
	for(int i = 0 ; i<MAX_MINOR ;i++){
		if(global_variable[i].data){
                      kfree(global_variable[i].data);
		      global_variable[i].data = NULL;
		};
		global_variable[i].devt = 0 ;
		global_variable[i].fops = NULL;
		global_variable[i].total_data_size = 0;
		global_variable[i].driver_class = NULL;
		global_variable[i].module_devices = NULL;
	};

	return 0;
};

static int allocator(void){
	pr_info("IN THE ALLOCATOR \n");
	int in;
	dev_t devvt;

        int major , minor ;

//--------------------------------------------------------------------------------------------------//
	in = alloc_chrdev_region(&devvt , 0 , MAX_MINOR ,"custom_driver");
	if(in<0){
		pr_err("ERROR IN CREATING THE BASE DEVT /n");
		return in;
	};

	for(int i = 0 ; i<MAX_MINOR ; i++){
		global_variable[i].devt  = devvt ;
	};
	pr_info("THE BASE DEVT HAS BEEN GENERATED\n");   
//--------------------------------------------------------------------------------------------------//
	major = MAJOR(devvt);
//--------------------------------------------------------------------------------------------------//
        for(int i =0 ; i<MAX_MINOR ; i++){
		global_variable[i].devt = MKDEV(major,i);
		cdev_init(&global_variable[i].cdev , &fops);
		global_variable[i].cdev.owner = THIS_MODULE ;
		in = cdev_add(&global_variable[i].cdev,global_variable[i].devt,i);
		if(in<0){
			pr_err("cdev creation error");
			goto fail_cdev ;
		};
	};
//--------------------------------------------------------------------------------------------------//

//--------------------------------------------------------------------------------------------------//
        struct class *dri_clss;
	dri_clss = class_create("my_custom_driver");
	if(IS_ERR(dri_clss)){
		pr_err("ERROR IN CREATING THE CLASS (error code : %ld) \n",PTR_ERR(dri_clss));
		goto fail_class ;
	};

	for(int i = 0 ; i<MAX_MINOR ; i++){
		global_variable[i].driver_class = dri_clss;
	};
	pr_info("CLASS HAS BEEN CREATED AND SAVED ACCROSS FOR ALL THE DEVICES \n");
	//------------------------------------------------------------------------//
	
	for(int i = 0 ; i<MAX_MINOR ; i++){
		global_variable[i].module_devices = device_create(global_variable[i].driver_class,NULL,global_variable[i].devt,NULL,"character_cus_driver %d",i);
		if(IS_ERR(global_variable[i].module_devices)){
			pr_err("ERROR IN CREATING THE DEVICE FILE (error code : %ld)\n", PTR_ERR(global_variable[i].module_devices));
			goto fail_device;
		};
	};
	pr_info("ALL THE MINOR DEVICES HAVE BEEN CREATED AND THERE DEV_t HAS BEEN UPDTED ACCORDINGLY \n");
//--------------------------------------------------------------------------------------------------//
        return 0;
//--------------------------------------------------------------------------------------------------//

fail_device :
	for(int i =0 ; i<MAX_MINOR ; i++){
		device_destroy(global_variable[i].driver_class, global_variable[i].devt);
	};

fail_class :
	class_destroy(global_variable[0].driver_class);
	pr_info("the class has been destroyed \n");

fail_cdev :
	unregister_chrdev_region(global_variable[0].devt ,MAX_MINOR);
	pr_info("the dev_t has been unregistered \n");


	int cleaner ;
	cleaner =  default_state();
//--------------------------------------------------------------------------------------------------//
	return -1;
};



static int __init  my_driver_init(void){
	pr_info("ENTER THE KERNEL CODE INIT \n");
	int res;

	res = allocator();
	if(res<0){
		pr_info("OPERATION FAILED \n");
		return -1 ;
	};

	pr_info("OPERATION COMPLETED \n");
	return 0;
};

static void __exit my_driver_exit(void){ 
        pr_info("EXITING FROM THE DRIVER \n");
 
	for(int i=0 ; i<MAX_MINOR ; i++){
                 device_destroy(global_variable[i].driver_class,global_variable[i].devt);
                 pr_info("device %d destroyed \n",i);
      }
      class_destroy(global_variable[0].driver_class);
      pr_info("CLASS DESTROYED \n");
      unregister_chrdev_region(global_variable[0].devt , MAX_MINOR);
      pr_info("MAJOR:MINOR DELETED AND DESTROYED FROM SYS/DEVICES \n");
};

module_init(my_driver_init);
module_exit(my_driver_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("blackstar");
MODULE_DESCRIPTION("a simple character driver with custom read and write");


