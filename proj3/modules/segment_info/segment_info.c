#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>

#include <linux/string.h>

#include <linux/pid.h>
#include <linux/sched.h>

#define BUFFER_SIZE 20

int segment_info_init(void);
ssize_t segment_info_write(struct file *filp, const char __user *ubuf, size_t count, loff_t *f_pos);
void segment_info_exit(void);

struct file_operations fops = {
    .owner = THIS_MODULE, 
    .write = segment_info_write,
};

struct miscdevice segment_info = {
    .minor = MISC_DYNAMIC_MINOR, 
    .name = "segment_info",
    .fops = &fops,
};


int segment_info_init(void) {
    int error;

    error = misc_register(&segment_info);
    if (error) {
        return error;
    }

    return 0;
}

/*
* only care about the second and third paramters. 
* ubuf is a pointer to the input (PID in this case)
* ubuf is in the user space, need to copy it to kernel space.
*
* count is the size of the input in bytes 
* check if count < buffer size (20)
*
* return count bytes on success. -1 otherwise. 
*
* change the error messages eventually 
*/
ssize_t segment_info_write(struct file *filp, const char __user *ubuf, size_t count, loff_t *f_pos) {
    char kbuf[BUFFER_SIZE]; 
    struct task_struct *input_task_tcb;

    int input_task_pid; 
    char *clean_kbuf;

    unsigned long code_section_size; 
    unsigned long data_section_size; 

    if (count > BUFFER_SIZE) {
        printk("segment_info: error - count > buffer_size\n");
        return -1;
    }

    if (copy_from_user(kbuf, ubuf, count) != 0) {
        printk("segment_info: error - copy_from_user()\n");
        return -1; 
    }

    clean_kbuf = strstrip(kbuf);
    
    if (kstrtoint(clean_kbuf, 0, &input_task_pid)) {
        printk("segment_info: error - kstrtoint()\n");
        return -1;
    }

    input_task_tcb = pid_task(find_vpid(input_task_pid), PIDTYPE_PID);

    if (input_task_tcb == 0) {
        printk("segment_info: error - task with pid %d not found\n", input_task_pid);
        return -1;
    }

    if (input_task_tcb->mm == 0) {
        printk("segment_info: error - mm is set to null\n");
        return -1;
    }

    code_section_size = input_task_tcb->mm->end_code - input_task_tcb->mm->start_code;
    data_section_size = input_task_tcb->mm->end_data - input_task_tcb->mm->start_data;

    printk("[Memory segment addresses of process %d]\n", input_task_pid);
    printk("%08lx - %08lx: code segment (%lu bytes)\n", input_task_tcb->mm->start_code, input_task_tcb->mm->end_code, code_section_size);
    printk("%08lx - %08lx: data segment (%lu bytes)\n", input_task_tcb->mm->start_data, input_task_tcb->mm->end_data, data_section_size);

    return count;
}

void segment_info_exit(void) {
    misc_deregister(&segment_info);
}

module_init(segment_info_init);
module_exit(segment_info_exit);
MODULE_LICENSE("GPL");