#include <linux/syscalls.h>
#include <asm/unistd.h>
#include <linux/signal.h>
#include <linux/sched.h>
#include <linux/uaccess.h>

int __sys_count_rt_tasks(int *result) {
    int counter = 0; 
    struct task_struct *g, *p;

    int fail_bytes = 0; 

    if (!result) { //null pointer is passed
        return -1;
    }

    for_each_process_thread(g, p) {
        if (p->rt_priority >= 1 && p->rt_priority <= 99) {
            counter++;
        }
    } 

    fail_bytes = copy_to_user(result, &counter, sizeof(int));

    if (fail_bytes != 0) { //copy failed
        return -1;
    }

    printk(KERN_ALERT "count_rt_tasks: %d", counter);

    return 0;
}

SYSCALL_DEFINE1(count_rt_tasks, int*, result) {
    return __sys_count_rt_tasks(result);
}
