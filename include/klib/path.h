//
// Created by linus on 06.07.25.
//

#ifndef PATH_H
#define PATH_H
#include <stddef.h>

size_t split_path(const char* path, char components[][32], size_t max_components);

#endif //PATH_H
