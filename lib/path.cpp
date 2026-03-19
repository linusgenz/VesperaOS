//
// Created by linus on 06.07.25.
//

#include <klib/string.h>
#include <vespera/types.h>
#include <vespera/mm/memory.h>

//  "/foo/bar/baz" -> "foo", "bar", "baz"
usize split_path(const char* path, char components[][32], const usize max_components) {
    usize count = 0;
    usize pos = 0;


    while (path[pos] == '/') pos++;
    usize comp_pos = 0;

    while (path[pos] != '\0' && count < max_components) {
        const char c = path[pos];
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

void normalize_path(const char* in, char* out, const usize out_size) {
    char segments[16][64];
    int depth = 0;

    const char* p = in;
    while (*p) {
        while (*p == '/') p++;
        if (!*p) break;

        const char* start = p;
        while (*p && *p != '/') p++;
        usize len = p - start;

        if (len == 1 && start[0] == '.') {
            continue;
        } else if (len == 2 && start[0] == '.' && start[1] == '.') {
            if (depth > 0) depth--;
        } else if (depth < 16 && len < 64) {
            memcpy(segments[depth], start, len);
            segments[depth][len] = '\0';
            depth++;
        }
    }

    usize pos = 0;
    out[pos++] = '/';
    for (int i = 0; i < depth; i++) {
        if (i > 0) out[pos++] = '/';
        const usize clen = strlen(segments[i]);
        if (pos + clen + 2 >= out_size) break;
        memcpy(out + pos, segments[i], clen);
        pos += clen;
    }
    out[pos] = '\0';
}
