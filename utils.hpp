
#ifndef __UTILS_HPP__
#define __UTILS_HPP__

#include "opts.hpp"

typedef unsigned long long ticks_t;

long get_ticks_res(); // Returns ticks resolution in nanoseconds
ticks_t get_ticks();
ticks_t get_ticks();
double ticks_to_secs(ticks_t ticks);
double ticks_to_ms(ticks_t ticks);
double ticks_to_us(ticks_t ticks);
ticks_t secs_to_ticks(double secs);

struct latency_t {
    ticks_t time;
    ticks_t duration;
};

void check(const char *str, int error);

typedef void* rnd_gen_t;
rnd_gen_t init_rnd_gen();
void free_rnd_gen(rnd_gen_t rnd_gen);
off64_t get_random(rnd_gen_t rnd_gen, rnd_dist_t dist, off64_t length, int sigma);

off64_t get_device_length(const char* device);

void drop_caches(const char *device);

// One pass standard deviation implementation: http://www.cs.berkeley.edu/~mhoemmen/cs194/Tutorials/variance.pdf
struct std_dev_t {
    float mk, qk;
    unsigned long long k;
};
void init_std_dev(std_dev_t *std_dev);
void add_to_std_dev(std_dev_t *std_dev, double x);
double get_variance(std_dev_t *std_dev);

#endif // __UTILS_HPP__

