//
// Created by Linus on 20.07.25.
//

#ifndef MUTEX_H
#define MUTEX_H

#include "../scheduling/thread.h"

namespace kernel {

    struct mutex_t {
        volatile bool locked;
        kthread_t* waiters;
    };

    void mutex_init(mutex_t* m);
    void mutex_lock(mutex_t* m);
    void mutex_unlock(mutex_t* m);

}

#endif //MUTEX_H
