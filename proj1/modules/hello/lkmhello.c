#include <linux/init.h>
#include <linux/module.h>
#include <linux/syscalls.h>

int hello_init(void)
{
	printk(KERN_ALERT "Hello world! team09 in kernel space\n");
	return 0;
}

void hello_exit(void)
{
}

module_init(hello_init)
module_exit(hello_exit)
MODULE_LICENSE("GPL");
