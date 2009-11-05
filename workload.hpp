
#ifndef __WORKLOAD_HPP__
#define __WORKLOAD_HPP__

int is_done(off64_t offset, workload_config_t *config);
off64_t prepare_offset(long long ops, rnd_gen_t rnd_gen,
                       workload_config_t *config);


#endif // __WORKLOAD_HPP__

