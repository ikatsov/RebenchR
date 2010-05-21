
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
                 long long min_ops_per_sec, long long max_ops_per_sec, float agg_std_dev) {
    if(config->duration_unit == dut_interactive)
        return;
    float total_secs = ticks_to_secs(end_time - start_time);
    if(config->silent) {
        printf("%d %.2f %llu %llu %d\n",
               (int)((float)ops / total_secs),
               ((double)ops * config->block_size / 1024 / 1024) / total_secs,
               min_ops_per_sec, max_ops_per_sec, (int)agg_std_dev);
    } else {
        float mean = (float)ops / total_secs;
        printf("Ops/sec: mean - %d (%.2f MB/sec), min - %llu, max - %llu, stddev - %d (%.2f%%)\n",
               (int)mean,
               ((double)ops * config->block_size / 1024 / 1024) / total_secs,
               min_ops_per_sec, max_ops_per_sec, (int)agg_std_dev,
               agg_std_dev / mean * 100.0f);
        printf("Total: %lld ops in %.2f secs\n", ops, ticks_to_secs(end_time - start_time));
    }
}

long long compute_total_ops(workload_simulation_t *ws) {
    long long ops = 0;
    for(int i = 0; i < ws->config.threads; i++) {
        ops += __sync_fetch_and_add(&(ws->engines[i]->ops), 0);
    }
    return ops;
}


