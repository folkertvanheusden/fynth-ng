#include <math.h>

static double encode_distance(const double initial, const double current)
{
        double distance = fabs(fabs(initial) - fabs(current));

        return 1.0 - distance;
}

void encode_surround(const double in, const double x, const double y, double *const left, double *const right, double *const back)
{
        if (y >= -1.0)
        {
                *right = (0.5 - encode_distance(-1.0, x) * 0.5) * ((y + 1.0) / 2.0) * in;

                *left = (0.5 + encode_distance(1.0, x) * 0.5) * ((y + 1.0) / 2.0) * in;
        }
        else
        {
                *left = *right = 0;
        }

        if (y < 0.0)
                *back = encode_distance(-1.0, y) * in;
        else
                *back = 0;
}
