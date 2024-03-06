#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>

#include <sys/mman.h>

long long timespec_to_ns(struct timespec t)
{
	return (t.tv_sec * 1000000000 + t.tv_nsec);
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("%s: <num. of bytes>", argv[0]);
        return -1;
    }

    struct timespec t1, t2;
    long long t1_ns, t2_ns, total_time_ns;
    int error;

    int n = atoi(argv[1]);
    
    char *buf = (char*)malloc(n);
    error = mlock(buf, n);

    if (error == -1) {
        printf("mlock failed\n");
        return -1;
    }

    clock_gettime(CLOCK_MONOTONIC, &t1);
    for (int i = 0; i < n; i += 4096) {
        buf[i] = 1;
    }
    clock_gettime(CLOCK_MONOTONIC, &t2);

    t1_ns = timespec_to_ns(t1);
    t2_ns = timespec_to_ns(t2);
    total_time_ns = t2_ns - t1_ns;

    printf("PID %d, %lld ns\n", getpid(), total_time_ns);

    free(buf);
    pause();

    return 0;
}