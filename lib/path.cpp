//
// Created by linus on 06.07.25.
//

#include "stddef.h"

//  "/foo/bar/baz" -> "foo", "bar", "baz"
size_t SplitPath(const char* path, char components[][32], size_t maxComponents) { // TODO components arr might be to small for some dir names
    size_t count = 0;
    size_t pos = 0;

    while (path[pos] == '/') pos++;

    size_t compPos = 0;

    while (path[pos] != '\0' && count < maxComponents) {
        char c = path[pos];
        if (c == '/') {
            if (compPos > 0) {
                components[count][compPos] = '\0';
                count++;
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
        count++;
    }

    return count;
}
