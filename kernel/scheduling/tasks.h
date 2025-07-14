//
// Created by Linus on 12.07.25.
//

#ifndef TASKS_H
#define TASKS_H
#include "stddef.h"
#include "stdint.h"

#define MAX_TASKS 128
#define STACK_SIZE 4096

struct Task {
    uint64_t id;
    bool active;
    void* stack;
    void (*entry)(void);
};

struct TaskManager {
    Task tasks[MAX_TASKS];
    size_t task_count;
    size_t current_task_index;
    uint64_t task_id_counter;
};

TaskManager task_manager;

#endif //TASKS_H
