
#ifndef __OPERATIONS_HPP__
#define __OPERATIONS_HPP__

int perform_op(int fd, void *mmap, char *buf, off64_t length, long long ops, rnd_gen_t rnd_gen,
               workload_config_t *config);

#endif // __OPERATIONS_HPP__

