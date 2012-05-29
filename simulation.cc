
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

void setup_io(workload_config_t *config, workload_simulation_t *ws, io_engine_t *io_engine) {
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

    // Setup output file if necessary
    if(config->output_file[0] != 0) {
        ws->output_fd = open(config->output_file, O_CREAT | O_TRUNC | O_APPEND | O_WRONLY, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
        check("Error opening the data file", ws->output_fd == -1);
    }
}

void cleanup_io(workload_config_t *config, workload_simulation_t *ws, io_engine_t *io_engine) {
    io_engine->pre_close_teardown();
    int res = close(io_engine->fd);
    check("Could not close the file", res == -1);
    
    if(ws->output_fd != -1) {
        res = close(ws->output_fd);
        check("Could not close the output file", res == -1);
    }
}

void* simulation_worker(void *arg) {
    io_engine_t *io_engine = (io_engine_t*)arg;
    io_engine->run_benchmark();
    return NULL;
}

void print_stats(ticks_t start_time, ticks_t end_time, long long ops, workload_config_t *config,
                 long long min_ops_per_sec, long long max_ops_per_sec, float agg_std_dev,
                 unsigned long long sum_latency, unsigned long long min_latency,
                 unsigned long long max_latency) {
    if(config->duration_unit == dut_interactive)
        return;
    float total_secs = ticks_to_secs(end_time - start_time);
    float _sum_latency;
    float _min_latency;
    float _max_latency;
    float _agg_std_dev;
    bool latency_use_ms = ticks_to_us(sum_latency / ops) > 1000.0f;
    if(latency_use_ms) {
        _sum_latency = ticks_to_ms(sum_latency);
        _min_latency = ticks_to_ms(min_latency);
        _max_latency = ticks_to_ms(max_latency);
        _agg_std_dev = ticks_to_ms(agg_std_dev);
    } else {
        _sum_latency = ticks_to_us(sum_latency);
        _min_latency = ticks_to_us(min_latency);
        _max_latency = ticks_to_us(max_latency);
        _agg_std_dev = ticks_to_us(agg_std_dev);
    }

    if(config->silent) {
        if(config->sample_step == 0) {
            printf("%.2f %.2f %.2f %.2f %.2f\n",
                   ticks_to_us(sum_latency) / ops,                                     // mean latency (in microseconds)
                   ((double)ops * config->block_size / 1024 / 1024) / total_secs,      // MB/sec
                   ticks_to_us(min_latency), ticks_to_us(max_latency),                 // min latency, max latency
                   ticks_to_us(agg_std_dev));                                          // deviation
        } else {
            printf("%d %.2f %llu %llu %d\n",
                   (int)((float)ops / total_secs),                                     // mean ops per sec
                   ((double)ops * config->block_size / 1024 / 1024) / total_secs,      // MB/sec
                   min_ops_per_sec, max_ops_per_sec, (int)agg_std_dev);                // min ops/sec, max ops/sec, deviation
        }
    } else {
        if(config->sample_step == 0) {
            float mean = _sum_latency / ops;
            printf("Latency: mean - %.2f", mean);
            if(latency_use_ms)
                printf("ms");
            else
                printf("us");
            printf(" (%.2f MB/sec), min - %.2f, max - %.2f, stddev - %.2f (%.2f%%)\n",
                   ((double)ops * config->block_size / 1024 / 1024) / total_secs,
                   _min_latency, _max_latency, _agg_std_dev,
                   _agg_std_dev / mean * 100.0f);
            printf("Mean ops/sec: %d\n", (int)((float)ops / total_secs));
        } else {
            float mean = (float)ops / total_secs;
            printf("Ops/sec: mean - %d (%.2f MB/sec), min - %llu, max - %llu, stddev - %d (%.2f%%)\n",
                   (int)mean,
                   ((double)ops * config->block_size / 1024 / 1024) / total_secs,
                   min_ops_per_sec, max_ops_per_sec, (int)agg_std_dev,
                   agg_std_dev / mean * 100.0f);
            float latency = 1000000.0f / mean;
            if(latency > 1000.0f) {
		printf("Sec/ops mean: %.2f ms/op\n", 1000.0f / mean);
            } else {
		printf("Sec/ops mean: %.2f us/op\n", 1000000.0f / mean);
            }
        }
        printf("Total: %lld ops in %.2f secs\n", ops, ticks_to_secs(end_time - start_time));
    }
}

void print_latency_stats(workload_config_t *config, stat_data_t stat_data) {
	if(config->duration_unit == dut_interactive)
        	return;

	printf("Latency statistics: mean - %.3f us, min - %.3f us, max - %.3f us | percentiles: ", 
		stat_data.mean / 1000.0, ticks_to_us(stat_data.min_value), ticks_to_us(stat_data.max_value));	
	for(std::map<double, ticks_t>::iterator it = stat_data.percentiles.begin(); it != stat_data.percentiles.end(); ++it) {
		printf("%.1fth - %.1f us; ", it->first*100, ticks_to_us(it->second) );
	}
	printf("\n");
}

long long compute_total_ops(workload_simulation_t *ws) {
    long long ops = 0;
    for(int i = 0; i < ws->config.threads; i++) {
        ops += __sync_fetch_and_add(&(ws->engines[i]->ops), 0);
    }
    return ops;
}


