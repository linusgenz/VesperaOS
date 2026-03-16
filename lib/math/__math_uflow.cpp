// Copyright: see musl copyright notice

#include <vespera/types.h>
#include <klib/math/libm.h>

extern "C" {

double __math_uflow(uint32_t sign)
{
    return __math_xflow(sign, 0x1p-767);
}

}