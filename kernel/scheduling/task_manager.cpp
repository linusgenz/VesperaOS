//
// Created by Linus on 12.07.25.
//

#include "tasks.h"
#include "../memory/heap.h"

void init_task_manager() {
    task_manager.task_count = 0;
    task_manager.current_task_index = 0;
    task_manager.task_id_counter = 1;  // Starts at 1
}

Task* create_task(void (*entry)(void)) {
    if (task_manager.task_count >= MAX_TASKS) {
        return nullptr;
    }
    Task* task = &task_manager.tasks[task_manager.task_count++];
    task->id = task_manager.task_id_counter++;
    task->active = true;
    task->entry = entry;
    task->stack = malloc(STACK_SIZE);  // alloc stack
    return task;
}

void remove_task(Task* task) {
    task->active = false;
    free(task->stack);
    // Weitere Logik, um das Task-Array zu komprimieren
}