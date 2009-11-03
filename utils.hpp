
#ifndef __UTILS_HPP__
#define __UTILS_HPP__

#include "opts.hpp"

typedef unsigned long long ticks_t;

ticks_t get_ticks();
float ticks_to_secs(ticks_t ticks);
ticks_t secs_to_ticks(float secs);

void check(const char *str, int error);

typedef void* rnd_gen_t;
rnd_gen_t init_rnd_gen();
void free_rnd_gen(rnd_gen_t rnd_gen);
off64_t get_random(rnd_gen_t rnd_gen, rnd_dist_t dist, off64_t length, int sigma);

off64_t get_device_length(const char* device);

void drop_caches(const char *device);

#endif // __UTILS_HPP__

