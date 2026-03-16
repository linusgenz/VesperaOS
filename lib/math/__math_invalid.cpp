// Copyright: see musl copyright notice

extern "C" {

double __math_invalid(double x)
{
    return (x - x) / (x - x);
}

}