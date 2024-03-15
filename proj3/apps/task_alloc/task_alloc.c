#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BUFFER_SIZE 500

struct task {
    char name[21]; 
    int C; 
    int T; 

    float util; 
};

struct bin {
    float capacity;

    int task_list_count;
    int task_list_size; 
    struct task *task_list;
};

int compare(const void *x, const void *y); 

void bin_task_list_del(struct bin bins[], int num_of_bins);
void bin_task_list_add(struct bin *list, struct task new_task); 

int BFD_heuristic(struct bin *CPUs, struct task *tasks, int num_of_bins, int num_of_tasks);
int WFD_heuristic(struct bin *CPUs, struct task *tasks, int num_of_bins, int num_of_tasks);
int FFD_heuristic(struct bin *CPUs, struct task *tasks, int num_of_bins, int num_of_tasks);

void print_result(struct bin *CPU, int num_of_bins);

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("%s: <filename.txt>\n", argv[0]);
        return -1;
    }

    FILE *fp = fopen(argv[1], "r");
    char buffer[BUFFER_SIZE];
    char *info;

    int num_of_bins = 0; 
    char policy[] = "ZZZ"; 
    int num_of_tasks = 0;

    struct task my_task; 

    int num_of_tasks_check = 0;
    int pass = 0;

    // check if file exists
    if (fp == 0) {
        return -1; 
    } 

    fgets(buffer, BUFFER_SIZE, fp); // get first line of file
    info = strtok(buffer, ","); // get number of bins
    num_of_bins = atoi(info);

    info = strtok(0, ","); // get heuristic policy
    strcpy(policy, info);
    policy[strcspn(policy, "\n")] = 0; //removes newline char ( thank u fgets() )

    fgets(buffer, BUFFER_SIZE, fp); // get second line of file
    info = strtok(buffer, ","); // get number of tasks
    num_of_tasks = atoi(info);

    // make sure parameters are valid
    if (num_of_bins <= 0 || num_of_tasks <= 0) {
        return -1;
    }

    // set up data structures 
    struct task *tasks = malloc(sizeof(struct task) * num_of_tasks);
    struct bin *CPU = malloc(sizeof(struct bin) * num_of_bins);

    // initialize tasks array to input tasks
    while (fgets(buffer, BUFFER_SIZE, fp) != 0 && num_of_tasks_check < num_of_tasks) {
        info = strtok(buffer, ",");
        strcpy(tasks[num_of_tasks_check].name, info);
        info = strtok(0, ",");
        tasks[num_of_tasks_check].C = atoi(info);
        info = strtok(0, ",");
        tasks[num_of_tasks_check].T = atoi(info);

        tasks[num_of_tasks_check].util = (float)tasks[num_of_tasks_check].C / (float)tasks[num_of_tasks_check].T;

        num_of_tasks_check++;
    }

    if (num_of_tasks > num_of_tasks_check) { // user input more tasks than listed in file
        num_of_tasks = num_of_tasks_check;
    }

    // initialize CPUs 
    for (int i = 0; i < num_of_bins; i++) {
        CPU[i].capacity = 0;            // empty bin (0 = empty, 1 = full)

        CPU[i].task_list_count = 0;     // current CPU task count
        CPU[i].task_list_size = 10;     // initial list size
        CPU[i].task_list = (struct task*)malloc(CPU[i].task_list_size * sizeof(struct task));
    }

    // sort tasks in tasks array from largest to smallest
    qsort(tasks, num_of_tasks, sizeof(struct task), compare);

    // at this point we're ready to run the algorithm
    if (strcmp(policy, "BFD") == 0) {
        pass = BFD_heuristic(CPU, tasks, num_of_bins, num_of_tasks);
    }
    else if (strcmp(policy, "WFD") == 0) {
        pass = WFD_heuristic(CPU, tasks, num_of_bins, num_of_tasks);
    }
    else if (strcmp(policy, "FFD") == 0) {
        pass = FFD_heuristic(CPU, tasks, num_of_bins, num_of_tasks);
    }

    // array iteration is done backwards to match example output
    if (pass == 0) {
        print_result(CPU, num_of_bins);
    }

    // dynamic mem cleanup
    bin_task_list_del(CPU, num_of_bins);
    free(tasks);
    free(CPU);
    
    fclose(fp);

    return 0;
}

void bin_task_list_del(struct bin bins[], int num_of_bins) {

    for (int i = 0; i < num_of_bins; i++) {
        free(bins[i].task_list);
        bins[i].task_list = 0;
        bins[i].task_list_size = 0;
        bins[i].task_list_count = 0;
    }
}

// adds element to the end of an array
void bin_task_list_add(struct bin *list, struct task new_task) {
    if (list->task_list_count == list->task_list_size) {
        list->task_list_size *= 2;
        list->task_list = realloc(list->task_list, list->task_list_size * sizeof(struct task));
    }
    list->task_list[list->task_list_count++] = new_task;
}

int WFD_heuristic(struct bin *CPUs, struct task *tasks, int num_of_bins, int num_of_tasks) {
    struct bin *chosen_bin; // pointer to chosen bin (CPU)

    float test_capacity; 
    float min_capacity;

    for (int i = 0; i < num_of_tasks; i++) { // loop through tasks (tasks)    
        chosen_bin = 0;                
        min_capacity = 100.0f;        
        for (int j = 0; j < num_of_bins; j++) { // for each task look at all bins (CPUs)
            test_capacity = CPUs[j].capacity + tasks[i].util; 
            if (test_capacity < min_capacity && test_capacity <= 1.0) {
                chosen_bin = &(CPUs[j]);
                min_capacity = CPUs[j].capacity + tasks[i].util;
            }
        }

        if (chosen_bin != 0) { // found a "optimal" bin to fit task into
            chosen_bin->capacity += tasks[i].util;
            bin_task_list_add(chosen_bin, tasks[i]);

        }
        else {
            printf("Failure\n");
            return -1;
        }
    }
    printf("Success\n");
    return 0;
}

int BFD_heuristic(struct bin *CPUs, struct task *tasks, int num_of_bins, int num_of_tasks) {
    struct bin *chosen_bin; // pointer to chosen bin (CPU)

    float test_capacity; 
    float max_capacity;

    for (int i = 0; i < num_of_tasks; i++) { // loop through tasks (tasks)    
        chosen_bin = 0;                
        max_capacity = -100.0f;        
        for (int j = 0; j < num_of_bins; j++) { // for each task look at all bins (CPUs)
            test_capacity = CPUs[j].capacity + tasks[i].util; 
            if (test_capacity > max_capacity && test_capacity <= 1.0) {
                chosen_bin = &(CPUs[j]);
                max_capacity = CPUs[j].capacity + tasks[i].util;
            }
        }
    
        if (chosen_bin != 0) { // found a "optimal" bin to fit task into
            chosen_bin->capacity += tasks[i].util;
            bin_task_list_add(chosen_bin, tasks[i]);

        }
        else {
            printf("Failure\n");
            return -1;
        }
    }
    printf("Success\n");
    return 0;
}

int FFD_heuristic(struct bin *CPUs, struct task *tasks, int num_of_bins, int num_of_tasks) {
    struct bin *chosen_bin; // pointer to chosen bin (CPU)
    float test_capacity;

    for (int i = 0; i < num_of_tasks; i++) { // loop through tasks (tasks)    
        chosen_bin = 0;                      
        for (int j = 0; j < num_of_bins; j++) { // for each task look at all bins (CPUs)
            test_capacity = CPUs[j].capacity + tasks[i].util; 
            if (test_capacity <= 1.0) {
                chosen_bin = &(CPUs[j]);
                break;
            }
        }

        if (chosen_bin != 0) { // found a "optimal" bin to fit task into
            chosen_bin->capacity += tasks[i].util;
            bin_task_list_add(chosen_bin, tasks[i]);

        }
        else {
            printf("Failure\n");
            return -1;
        }
    }
    printf("Success\n");
    return 0;
}

void print_result(struct bin *CPU, int num_of_bins) {
    for (int i = 0; i < num_of_bins; i++) {
        printf("CPU%d", i);
        for (int j = CPU[i].task_list_count - 1; j >= 0; j--) {
            printf(",%s", CPU[i].task_list[j].name);
        }
        printf("\n");
    }

    return;
}

/*
* used with qsort. 
* sort order depends on what is returned. 
* in this case task list is sorted from largest to smallest.
* 
* return value is done this way to avoid floating point issues 
* and still allow compare() to return an int
*/
int compare(const void *x, const void *y) {
    struct task *task_x = (struct task*)x;
    struct task *task_y = (struct task*)y;

    float x_float = task_x->util; 
    float y_float = task_y->util;

    //return (x_float > y_float) - (x_float < y_float);
    return (x_float < y_float) - (x_float > y_float);
}

