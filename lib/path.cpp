//
// Created by linus on 06.07.25.
//

#include <klib/string.h>
#include <vespera/types.h>

//  "/foo/bar/baz" -> "foo", "bar", "baz"
usize split_path(const char* path, char components[][32], usize max_components) {
    usize count = 0;
    usize pos = 0;


    while (path[pos] == '/') pos++;
    usize comp_pos = 0;

    while (path[pos] != '\0' && count < max_components) {
        char c = path[pos];
        if (c == '/') {
            if (comp_pos > 0) {
                components[count][comp_pos] = '\0';

                // Handle . and ..
                if (strcmp(components[count], ".") == 0) {
                    // ignore
                } else if (strcmp(components[count], "..") == 0) {
                    if (count > 0) count--;
                } else {
                    count++;
                }
                comp_pos = 0;
            }
            while (path[pos] == '/') pos++;
            continue;
        }

        if (comp_pos < 31) {
            components[count][comp_pos++] = c;
        }
        pos++;
    }

    if (comp_pos > 0 && count < max_components) {
        components[count][comp_pos] = '\0';
        if (strcmp(components[count], ".") == 0) {
            // ignore
        } else if (strcmp(components[count], "..") == 0) {
            if (count > 0) count--;
        } else {
            count++;
        }
    }

    return count;
}
