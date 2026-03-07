//
// Created by linus on 06.07.25.
//

#ifndef PATH_H
#define PATH_H
#include <vespera/types.h>

usize split_path(const char* path, char components[][32], usize max_components);

#endif //PATH_H
