
#ifndef __SIMULATION_HPP__
#define __SIMULATION_HPP__

#include <vector>
#include <pthread.h>
#include "utils.hpp"

// Describes each workload simulation
class io_engine_t;
struct workload_simulation_t {
    workload_simulation_t()
        : min_ops_per_sec(1000000), max_ops_per_sec(0), last_ops_so_far(0)
        {}
    
    std::vector<io_engine_t*> engines;
    std::vector<pthread_t> threads;
    workload_config_t config;
    int is_done;
    ticks_t start_time, end_time;
    long long ops;

    void *mmap;

    // stats info
    long long min_ops_per_sec, max_ops_per_sec;
    unsigned long long last_ops_so_far;
    std_dev_t std_dev;
};
typedef std::vector<workload_simulation_t*> wsp_vector;

class io_engine_t;
void setup_io(workload_config_t *config, io_engine_t *io_engine);
void cleanup_io(workload_config_t *config, io_engine_t *io_engine);

void* simulation_worker(void *arg);

void print_stats(ticks_t start_time, ticks_t end_time, long long ops, workload_config_t *config,
                 long long min_ops_per_sec, long long max_ops_per_sec, float agg_std_dev);
long long compute_total_ops(workload_simulation_t *ws);

#endif // __SIMULATION_HPP__

