
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <errno.h>
#include <gsl/gsl_randist.h>
#include <fcntl.h>
#include <unistd.h>
#include "opts.hpp"
#include "utils.hpp"

ticks_t get_ticks() {
    timespec tv;
    clock_gettime(CLOCK_MONOTONIC, &tv);
    return secs_to_ticks(tv.tv_sec) + tv.tv_nsec;
}

float ticks_to_secs(ticks_t ticks) {
    return ticks / 1000000000.0f;
}

ticks_t secs_to_ticks(float secs) {
    return secs * 1000000000;
}

void check(const char *str, int error) {
    if (error) {
        if(errno == 0)
            errno = EINVAL;
        perror(str);
        exit(-1);
    }
}

rnd_gen_t init_rnd_gen() {
    gsl_rng *rng = gsl_rng_alloc(gsl_rng_mt19937);
    gsl_rng_set(rng, get_ticks());
    return (void*)rng;
}

void free_rnd_gen(rnd_gen_t rnd_gen) {
    gsl_rng_free((gsl_rng*)rnd_gen);
}

off64_t get_random(rnd_gen_t rnd_gen, rnd_dist_t dist, off64_t length, int sigma) {
    double tmp;
    switch(dist)
    {
    case rdt_uniform:
        return gsl_ran_flat((gsl_rng*)rnd_gen, 0, length);
        break;
        
    case rdt_normal:
        /* sigma == percent of the data one standard diviation away
         * from the mean. */
        tmp = gsl_ran_gaussian((gsl_rng*)rnd_gen, length / 100.0f * sigma) + (length / 2);
        if(tmp < 0)
            tmp = 0;
        if(tmp > length)
            tmp = length - 1;
        return tmp;
        break;
        
    case rdt_power:
        /* sigma == percent of the length which will end up being the
         * mean accessed location. */
        tmp = gsl_ran_exponential((gsl_rng*)rnd_gen, length / 100.0f * sigma);
        if(tmp > length)
            tmp = length - 1;
        return tmp;
        break;
        
    default:
        check("Invalid distribution", 1);
    }
}

off64_t get_device_length(const char* device) {
    int fd;
    off64_t length;
    
    fd = open64(device, O_RDONLY);
    check("Error opening device", fd == -1);

    length = lseek64(fd, 0, SEEK_END);
    check("Error computing device size", length == -1);

    int res = close(fd);
    check("Could not close the file", res == -1);

    return length;
}

