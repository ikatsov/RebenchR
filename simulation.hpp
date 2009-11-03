
#ifndef __SIMULATION_HPP__
#define __SIMULATION_HPP__

#include <vector>
#include <pthread.h>
#include "utils.hpp"

// Describes each thread in a workload
struct simulation_info_t {
    int *is_done;
    long ops;
    int fd;
    workload_config_t *config;
    void *mmap;
};

// Describes each workload simulation
struct workload_simulation_t {
    std::vector<simulation_info_t*> sim_infos;
    std::vector<pthread_t> threads;
    workload_config_t config;
    int is_done;
    int fd;
    ticks_t start_time, end_time;
    long long ops;
    void *mmap;
};
typedef std::vector<workload_simulation_t*> wsp_vector;

void setup_io(int *fd, void **map, workload_config_t *config);
void cleanup_io(int fd, void *map, workload_config_t *config);

void* simulation_worker(void *arg);

void print_stats(ticks_t start_time, ticks_t end_time, long long ops, workload_config_t *config);
long long compute_total_ops(workload_simulation_t *ws);

#endif // __SIMULATION_HPP__

