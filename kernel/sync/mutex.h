//
// Created by Linus on 20.07.25.
//

#ifndef MUTEX_H
#define MUTEX_H

class Unit;

namespace kernel {
    inline bool scheduling_started = false;
    struct mutex_t {
        volatile bool locked;
        Unit* waiters;
    };

    void mutex_init(mutex_t* m);
    void mutex_lock(mutex_t* m);
    void mutex_unlock(mutex_t* m);

}

#endif //MUTEX_H
