#include <stdio.h>
#include <unistd.h>
#include <sys/syscall.h>

int main(int argc, char **argv) {
    int task_count = -1;

    int error = syscall(451, &task_count); 

    if (error < 0) { //syscall error
        return -1;
    }

    printf("%d\n", task_count);

    return 0;
}

