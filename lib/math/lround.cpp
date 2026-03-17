// Copyright: see musl copyright notice
#include <klib/math/math.h>

extern "C" {

long lround(double x)
{
	return round(x);
}

}