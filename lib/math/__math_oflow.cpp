// Copyright: see musl copyright notice

#include <vespera/types.h>
#include <klib/math/libm.h>

extern "C" {

double __math_oflow(uint32_t sign)
{
    return __math_xflow(sign, 0x1p769);
}

}