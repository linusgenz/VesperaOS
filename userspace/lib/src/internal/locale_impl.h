#ifndef _LOCALE_IMPL_H
#define _LOCALE_IMPL_H

#include <bits/alltypes.h>
#include "features.h"
#include <locale.h>
#include <stdlib.h>

#define LOCALE_NAME_MAX 23

struct __locale_map {
    const void *map;
    size_t map_size;
    char name[LOCALE_NAME_MAX+1];
    const struct __locale_map *next;
};

extern hidden volatile int __locale_lock[1];

extern hidden const struct __locale_map __c_dot_utf8;
extern hidden const struct __locale_struct __c_locale;
extern hidden const struct __locale_struct __c_dot_utf8_locale;

hidden const struct __locale_map *__get_locale(int, const char *);
hidden const char *__mo_lookup(const void *, size_t, const char *);
hidden const char *__lctrans(const char *, const struct __locale_map *);
hidden const char *__lctrans_cur(const char *);
hidden const char *__lctrans_impl(const char *, const struct __locale_map *);
hidden int __loc_is_allocated(locale_t);
hidden char *__gettextdomain(void);



#endif
