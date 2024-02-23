#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>
#include <linux/time.h>

// for list_head
#include <linux/list.h>

// for pid_task() and find_vpid()
#include <linux/sched.h> 
#include <linux/pid.h>   

// for vmalloc
#include <linux/vmalloc.h> 

// for timer stuff 
#include <linux/hrtimer.h>
#include <linux/ktime.h> 

#define SET_RTMON               _IO(0, 100)
#define CANCEL_RTMON            _IO(0, 101)
#define WAIT_FOR_NEXT_PERIOD    _IO(0, 102)

int rtmon_init(void);
long rtmon_ioctl(struct file *file, unsigned int cmd, unsigned long arg);
ssize_t rtmon_read(struct file *flip, char __user *buf, size_t count, loff_t *f_pos);
void rtmon_exit(void);

/*
//this isnt going to work (probably) 
struct rtmon_param {
    int pid; 
    int C, T; // C: execution time in ms, T: period in ms

    struct list_head l; 
};
LIST_HEAD(rt_task_list); */

struct rtmon_param {
    int pid; 
    int C, T; // C: execution time in ms, T: period in ms
};

struct hrtimer;

struct list_rtmon_param {
    struct rtmon_param info; 
    struct hrtimer hr_timer;
    struct list_head l; 
};
LIST_HEAD(rt_task_list);

struct file_operations fops = {
    .owner = THIS_MODULE, 
    .read = rtmon_read,
    .unlocked_ioctl = rtmon_ioctl,
};

struct miscdevice rtmon = {
    .minor = MISC_DYNAMIC_MINOR, 
    .name = "rtmon",
    .fops = &fops,
};

long lkm_set_rtmon(struct rtmon_param *new_task);
long lkm_cancel_rtmon(int del_pid);
long lkm_wait_for_next_period(void); 

enum hrtimer_restart hrtimer_handler(struct hrtimer *timer) {
    struct task_struct *found_task;
    struct list_rtmon_param *ltp = container_of(timer, struct list_rtmon_param, hr_timer);
    ktime_t k_time = ms_to_ktime(ltp->info.T);

    printk("in timer handler of %d with C: %d and T: %d\n", ltp->info.pid, ltp->info.C, ltp->info.T);

    // check if the task still exists in the list
    found_task = pid_task(find_vpid(ltp->info.pid), PIDTYPE_PID);

    if (found_task != 0) { // task has been found active - reset the timer
        printk("timer handler: task is found active\n");
        hrtimer_forward(timer, ktime_get(), k_time);
        wake_up_process(found_task);
        return HRTIMER_RESTART;
    }
    else { //task is inactive - do not reset the timer
        printk("timer handler: task is found inactive\n");
        list_del(&ltp->l);
        vfree(ltp);
        return HRTIMER_NORESTART; 
    }
}

int rtmon_init(void) {
    int error;

    error = misc_register(&rtmon);
    if (error) {
        printk("rtmon: misc_register failed\n");
        return error;
    }

    printk("rtmon: registered\n");
    return 0;
}

long rtmon_ioctl(struct file *file, unsigned int cmd, unsigned long arg) {
    struct rtmon_param new_task;
    int del_pid; 

    switch(cmd) {
        case SET_RTMON:
            printk("rtmon: SET_RTMON\n");
            if (copy_from_user(&new_task, (struct rtmon_param*)arg, sizeof(struct rtmon_param)) != 0) {
                return -1; // error
            }
            return lkm_set_rtmon(&new_task);
            break;
        case CANCEL_RTMON:
            printk("rtmon: CANCEL_RTMON\n");
            if (copy_from_user(&del_pid, (int*)arg, sizeof(int)) != 0) {
                return -1; // error
            }
            return lkm_cancel_rtmon(del_pid);
            break;
        case WAIT_FOR_NEXT_PERIOD:
            printk("rtmon: WAIT_FOR_NEXT_PERIOD\n");
            return lkm_wait_for_next_period();
            break;
        default:
            return -EINVAL;
    }

    return 0;
}

long lkm_set_rtmon(struct rtmon_param *new_task) {
    struct list_rtmon_param *p; 
    struct task_struct *found_task;
    ktime_t k_time; 

    // parameter range checking
    if (new_task->C <= 0 || new_task->C > 10000 || new_task->T <= 0 || new_task->T > 10000 || new_task->C > new_task->T) {
        printk("lkm_set_rtmon: new_task has invalid parameters\n");
        return -1; 
    }

    // see if new_task->pid really exists
    found_task = pid_task(find_vpid(new_task->pid), PIDTYPE_PID);
    if (found_task == 0) { // null, not found
        printk("lkm_set_rtmon: new_task has an invalid pid\n");
        return -1; 
    }

    // new_task has valid parameters, is it already in the list?
    list_for_each_entry(p, &rt_task_list, l) {
        if (p->info.pid == new_task->pid) { // task has been found in the list
            printk("lkm_set_rtmon: new_task is already in the list\n");
            return -1;
        }
    }

    printk("lkm_set_rtmon: new_task is being added to the list\n");
    // add new task to list
    p = vmalloc(sizeof(struct list_rtmon_param));
    p->info.pid = new_task->pid;
    p->info.C = new_task->C;
    p->info.T = new_task->T;
    
    // start timer for new task
    k_time = ms_to_ktime(new_task->T);
    hrtimer_init(&(p->hr_timer), CLOCK_MONOTONIC, HRTIMER_MODE_REL);
    p->hr_timer.function = &hrtimer_handler;
    hrtimer_start(&(p->hr_timer), k_time, HRTIMER_MODE_REL);

    list_add(&p->l, &rt_task_list);

    return 0;
}

long lkm_cancel_rtmon(int del_pid) {
    struct list_rtmon_param *p, *tmp;

    list_for_each_entry_safe(p, tmp, &rt_task_list, l) {
        if (p->info.pid == del_pid) { // task has been found in the list
            printk("lkm_cancel_rtmon: task to be deleted has been found\n");
            hrtimer_cancel(&(p->hr_timer));
            list_del(&p->l);
            vfree(p);
            
            return 0;
        }
    }

    printk("lkm_cancel_rtmon: task to be deleted has not been found\n");
    // task with del_pid was not found
    return -1;
}

// print all tasks in the list, if a task in the list isnt found - then 
ssize_t rtmon_read(struct file *flip, char __user *buf, size_t count, loff_t *f_pos) {
    struct list_rtmon_param *p, *tmp;
    struct task_struct *found_task;

    printk("rtmon_read\n");

    list_for_each_entry_safe(p, tmp, &rt_task_list, l) {
        found_task = pid_task(find_vpid(p->info.pid), PIDTYPE_PID);

        if (found_task != 0) { // task has been found active - print it
            printk("print_rtmon: PID %d, C %d ms, T %d ms\n", p->info.pid, p->info.C, p->info.T);
        }
        else if (found_task == 0) { //task is in the list but inactive - delete it (add hrtimer_cancel here?)
            printk("print_rtmon: task is in the list but inactive\n");
            hrtimer_cancel(&(p->hr_timer));
            list_del(&p->l);
            vfree(p);
        }
    }

    return 0;
}

/*
 suspend the calling task, then wake it up at the beginning 
 of the next period.

 if the calling task has not been registed with rtmon, return -1. 
 otherwise, return 0.
*/
long lkm_wait_for_next_period(void) {
    struct list_rtmon_param *p;
    struct task_struct *calling_task;

    // get the TCB of the calling task
    calling_task = current;

    // check if the calling task has been registered with rtmon
    list_for_each_entry(p, &rt_task_list, l) {
        if (p->info.pid == calling_task->pid) { // task has been found in the list
            printk("lkm_wait_for_next_period: calling task found in the list\n");
            // suspend the calling task 
            set_current_state(TASK_INTERRUPTIBLE);
            schedule();
            return 0;
        }
    }

    // calling task not found in the list
    return -1;
}

void rtmon_exit(void) {
    struct list_rtmon_param *p, *tmp;
    
    // de-allocate dynamically allocated kernel memory
    list_for_each_entry_safe(p, tmp, &rt_task_list, l) {
        hrtimer_cancel(&(p->hr_timer));
        list_del(&p->l);
        vfree(p);
    }

    misc_deregister(&rtmon);
    printk(KERN_INFO "rtmon: deregistered\n");
}

module_init(rtmon_init);
module_exit(rtmon_exit);
MODULE_LICENSE("GPL");