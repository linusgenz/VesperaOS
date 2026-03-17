// Copyright: see musl copyright notice
#include <klib/math/math.h>

extern "C" {

long lroundf(float x)
{
	return roundf(x);
}

}