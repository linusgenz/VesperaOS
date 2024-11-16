//
// Created by linus on 04.11.24.
//

#ifndef CPU_H
#define CPU_H
#include <stdint.h>

extern "C" void get_cpu_vendor(char buf[13]);
extern "C" uint64_t check_cpu_features();
extern "C" void get_cpu_brand(char buf[49]);

#endif //CPU_H