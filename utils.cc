
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

long get_ticks_res() {
    timespec tv;
    clock_getres(CLOCK_MONOTONIC, &tv);
    return secs_to_ticks(tv.tv_sec) + tv.tv_nsec;
}

float ticks_to_secs(ticks_t ticks) {
    return ticks / 1000000000.0f;
}

float ticks_to_ms(ticks_t ticks) {
    return ticks / 1000000.0f;
}

ticks_t secs_to_ticks(float secs) {
    return (unsigned long long)secs * 1000000000L;
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
    case rdt_const:
        return 0;
        break;
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

void drop_caches(const char *device) {
    int res;
    int fd = open64(device, O_NOATIME | O_RDWR);
    check("Error opening device", fd == -1);

    res = posix_fadvise(fd, 0, 0, POSIX_FADV_DONTNEED);    
    check("Could not drop caches", res != 0);

    res = close(fd);
    check("Could not close the file", res == -1);
}

void init_std_dev(std_dev_t *std_dev) {
    std_dev->mk = 0;
    std_dev->qk = 0;
    std_dev->k = 0;
}

void add_to_std_dev(std_dev_t *std_dev, float x) {
    std_dev->k += 1;
    if(std_dev->k == 1) {
        std_dev->mk = x;
        std_dev->qk = 0;
    } else {
        float temp = x - std_dev->mk;
        std_dev->mk += (temp / std_dev->k);
        std_dev->qk += (((std_dev->k - 1) * temp * temp) / std_dev->k);
    }
}

float get_variance(std_dev_t *std_dev) {
    return std_dev->qk / std_dev->k;
}
