// locale.c
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 18.04.26.
//
// This file is part of VesperaOS.
//
// VesperaOS is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// VesperaOS is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with VesperaOS. If not, see <https://www.gnu.org/licenses/>.

#include <locale.h>
#include <limits.h>

static struct lconv c_locale = {
    .decimal_point     = ".",
    .thousands_sep     = "",
    .grouping          = "",
    .int_curr_symbol   = "",
    .currency_symbol   = "",
    .mon_decimal_point = "",
    .mon_thousands_sep = "",
    .mon_grouping      = "",
    .positive_sign     = "",
    .negative_sign     = "",
    .int_frac_digits   = CHAR_MAX,
    .frac_digits       = CHAR_MAX,
    .p_cs_precedes     = CHAR_MAX,
    .p_sep_by_space    = CHAR_MAX,
    .n_cs_precedes     = CHAR_MAX,
    .n_sep_by_space    = CHAR_MAX,
    .p_sign_posn       = CHAR_MAX,
    .n_sign_posn       = CHAR_MAX,
};

char* setlocale(int category, const char* locale) {
    (void)category;
    (void)locale;
    return "C";
}

struct lconv* localeconv(void) {
    return &c_locale;
}