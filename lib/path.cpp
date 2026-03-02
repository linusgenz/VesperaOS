//
// Created by linus on 06.07.25.
//

#include <string.h>

//  "/foo/bar/baz" -> "foo", "bar", "baz"
size_t split_path(const char* path, char components[][32], size_t maxComponents) {
    size_t count = 0;
    size_t pos = 0;


    while (path[pos] == '/') pos++;
    size_t compPos = 0;

    while (path[pos] != '\0' && count < maxComponents) {
        char c = path[pos];
        if (c == '/') {
            if (compPos > 0) {
                components[count][compPos] = '\0';

                // Handle . and ..
                if (strcmp(components[count], ".") == 0) {
                    // ignore
                } else if (strcmp(components[count], "..") == 0) {
                    if (count > 0) count--;
                } else {
                    count++;
                }
                compPos = 0;
            }
            while (path[pos] == '/') pos++;
            continue;
        }

        if (compPos < 31) {
            components[count][compPos++] = c;
        }
        pos++;
    }

    if (compPos > 0 && count < maxComponents) {
        components[count][compPos] = '\0';
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
