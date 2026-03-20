//
// Created by linus on 06.07.25.
//

#ifndef PATH_H
#define PATH_H
#include <vespera/types.h>

usize split_path(const char* path, char components[][32], usize max_components);

void normalize_path(const char* in, char* out, usize out_size);

void strip_trailing_slash(char* path);

#endif //PATH_H
