#include <linux/init.h>
#include <linux/module.h>
#include <linux/syscalls.h>

#include <asm/unistd.h>
#include <linux/signal.h>
#include <linux/sched.h>   
#include <linux/uaccess.h> 

extern void *sys_call_table[]; 

asmlinkage long (*original_rt_count_tasks)(int*);

//function is actually defined as __arm64_sys_mod_count_rt_tasks
SYSCALL_DEFINE1(mod_count_rt_tasks, int*, result) {
    int counter = 0; 
    struct task_struct *g, *p;

    int fail_bytes = 0;

    if (!result) { //null pointer is passed
        return -1;
    }

    for_each_process_thread(g, p) {
        if (p->rt_priority > 50 && p->rt_priority <= 99) {
            counter++;
        }
    } 

    fail_bytes = copy_to_user(result, &counter, sizeof(int));

    if (fail_bytes != 0) { //copy failed
        return -1;
    }

    printk("count_rt_tasks[MOD]: %d", counter);

    return 0;
}



int mod_count_tasks_init(void) {
    /*save the original syscall at index 451*/
    original_rt_count_tasks = sys_call_table[__NR_count_rt_tasks];

    /*modify syscall table at index 451*/
    sys_call_table[__NR_count_rt_tasks] = __arm64_sys_mod_count_rt_tasks;

    return 0;
}

void mod_count_tasks_exit(void) {
    /*restore the original syscall at index 451*/
    sys_call_table[__NR_count_rt_tasks] = original_rt_count_tasks;
    return;
}

module_init(mod_count_tasks_init);
module_exit(mod_count_tasks_exit);
MODULE_LICENSE("GPL");
