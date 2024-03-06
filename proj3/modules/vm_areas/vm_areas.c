#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>

#include <linux/string.h>

#include <linux/pid.h>
#include <linux/sched.h>

#include <linux/mm.h>

#define BUFFER_SIZE     20
#define PAGE_SIZE_KB    4

int vm_areas_init(void);
ssize_t vm_areas_write(struct file *filp, const char __user *ubuf, size_t count, loff_t *f_pos);
void vm_areas_exit(void);

struct file_operations fops = {
    .owner = THIS_MODULE, 
    .write = vm_areas_write,
};

struct miscdevice vm_areas = {
    .minor = MISC_DYNAMIC_MINOR, 
    .name = "vm_areas",
    .fops = &fops,
};


int vm_areas_init(void) {
    int error;

    error = misc_register(&vm_areas);
    if (error) {
        return error;
    }

    return 0;
}

ssize_t vm_areas_write(struct file *filp, const char __user *ubuf, size_t count, loff_t *f_pos) {
    char kbuf[BUFFER_SIZE]; 
    struct task_struct *input_task_tcb;

    int input_task_pid; 
    char *clean_kbuf;

    struct vm_area_struct *vma;
    unsigned long vma_size;

    int num_phys_mem_pages;
    int size_phys_mem_pages;
    
    pte_t *ptep; 
    spinlock_t *ptl;

    MA_STATE(mas, 0, 0, 0);

    if (count > BUFFER_SIZE) {
        printk("vm_areas: error - count > buffer_size\n");
        return -1;
    }

    if (copy_from_user(kbuf, ubuf, count) != 0) {
        printk("vm_areas: error - copy_from_user()\n");
        return -1; 
    }

    clean_kbuf = strstrip(kbuf);
    
    if (kstrtoint(clean_kbuf, 0, &input_task_pid)) {
        printk("vm_areas: error - kstrtoint()\n");
        return -1;
    }

    input_task_tcb = pid_task(find_vpid(input_task_pid), PIDTYPE_PID);

    if (input_task_tcb == 0) {
        printk("vm_areas: error - task with pid %d not found\n", input_task_pid);
        return -1;
    }

    if (input_task_tcb->mm == 0) {
        printk("vm_areas: error - mm is set to null\n");
        return -1;
    }

    printk("[Memory-mapped areas of process %d]\n", input_task_pid);

    mas.tree = &input_task_tcb->mm->mm_mt;
    mas_for_each(&mas, vma, ULONG_MAX) {
        vma_size = vma->vm_end - vma->vm_start;
        num_phys_mem_pages = 0;

        // iterate through the memory area in page sized strides. 
        for (unsigned long i = vma->vm_start; i < vma->vm_end; i += 4096) {
            if (follow_pte(vma->vm_mm, i, &ptep, &ptl) == 0) {
                num_phys_mem_pages++;
                pte_unmap_unlock(ptep, ptl);
            }
        } 

        size_phys_mem_pages = num_phys_mem_pages * PAGE_SIZE_KB;

        if (vma->vm_flags & VM_LOCKED) { 
            printk("%08lx -  %08lx: %lu bytes [L], %d pages (%d KB) in phymem\n", 
                vma->vm_start, vma->vm_end, vma_size, num_phys_mem_pages, size_phys_mem_pages);
        }
        else {
           printk("%08lx -  %08lx: %lu bytes, %d pages (%d KB) in phymem\n", 
                vma->vm_start, vma->vm_end, vma_size, num_phys_mem_pages, size_phys_mem_pages);
        }

    }

    return count;
}

void vm_areas_exit(void) {
    misc_deregister(&vm_areas);
}

module_init(vm_areas_init);
module_exit(vm_areas_exit);
MODULE_LICENSE("GPL");