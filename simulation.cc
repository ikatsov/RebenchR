
#include <aio.h>
#include <strings.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include "simulation.hpp"
#include "io_engine.hpp"

void setup_io(workload_config_t *config, io_engine_t *io_engine) {
    int res, fd;
    int flags = 0;
    
    if(!config->do_atime)
        flags |= O_NOATIME;
    
    if(config->direct_io)
        flags |= O_DIRECT;
    
    if(config->append_only)
        flags |= O_APPEND;

    flags |= io_engine->contribute_open_flags();
        
    fd = open64(config->device, flags);
    check("Error opening device", fd == -1);

    io_engine->fd = fd;

    io_engine->post_open_setup();
}

void cleanup_io(workload_config_t *config, io_engine_t *io_engine) {
    io_engine->pre_close_teardown();
    int res = close(io_engine->fd);
    check("Could not close the file", res == -1);
}

void* simulation_worker(void *arg) {
    io_engine_t *io_engine = (io_engine_t*)arg;
    io_engine->run_benchmark();
    return NULL;
}

void print_stats(ticks_t start_time, ticks_t end_time, long long ops, workload_config_t *config,
                 float min_op_time_in_ms, float max_op_time_in_ms, float op_total_ms,
                 float std_dev) {
    if(config->duration_unit == dut_interactive)
        return;
    float total_secs = ticks_to_secs(end_time - start_time);
    if(config->silent) {
        printf("%d %.2f ",
               (int)((float)ops / total_secs),
               ((double)ops * config->block_size / 1024 / 1024) / total_secs);
        if(config->stats_type == st_op) {
            printf("%.2f %.2f %.2f %.2f\n", min_op_time_in_ms, max_op_time_in_ms, op_total_ms / ops, std_dev);
        }
        else {
            printf("\n");
        }
    } else {
        printf("Operations/sec: %d (%.2f MB/sec) - %lld ops in %.2f secs\n",
               (int)((float)ops / total_secs),
               ((double)ops * config->block_size / 1024 / 1024) / total_secs,
               ops, ticks_to_secs(end_time - start_time));
        if(config->stats_type == st_op) {
            printf("Operation time stats: min - %.2fms, max - %.2fms, mean - %.2fms, stddev - %.2fms\n",
                   min_op_time_in_ms, max_op_time_in_ms,
                   op_total_ms / ops, std_dev);
        }
    }
}

long long compute_total_ops(workload_simulation_t *ws) {
    long long ops = 0;
    for(int i = 0; i < ws->config.threads; i++) {
        ops += __sync_fetch_and_add(&(ws->engines[i]->ops), 0);
    }
    return ops;
}


