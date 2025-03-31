#include "sampler.h"

#include "math.h"

#define TPH_POISSON_IMPLEMENTATION
#include "tph_poisson.h"

void sample_points(jcv_point* points, unsigned char* img_data, jcv_point* dimensions)
{
    const tph_poisson_real bounds_min[2] = {
        (tph_poisson_real)-10, (tph_poisson_real)-10 };
      const tph_poisson_real bounds_max[2] = {
        (tph_poisson_real)10, (tph_poisson_real)10 };

    const tph_poisson_args args = {
        .bounds_max = bounds_min,
        .bounds_max = bounds_max,
        .radius = 3,
        .ndims = 2,
        .max_sample_attempts = 30,
        .seed = 2002,
    };
}
