
#ifndef __SIMULATION_HPP__
#define __SIMULATION_HPP__

#include <vector>
#include <pthread.h>
#include "utils.hpp"

// Describes each workload simulation
class io_engine_t;
struct workload_simulation_t {
    std::vector<io_engine_t*> engines;
    std::vector<pthread_t> threads;
    workload_config_t config;
    int is_done;
    ticks_t start_time, end_time;
    long long ops;
    void *mmap;
};
typedef std::vector<workload_simulation_t*> wsp_vector;

class io_engine_t;
void setup_io(workload_config_t *config, io_engine_t *io_engine);
void cleanup_io(workload_config_t *config, io_engine_t *io_engine);

void* simulation_worker(void *arg);

void print_stats(ticks_t start_time, ticks_t end_time, long long ops, workload_config_t *config,
                 float min_op_time_in_ms, float max_op_time_in_ms, float op_total_ms);
long long compute_total_ops(workload_simulation_t *ws);

#endif // __SIMULATION_HPP__

