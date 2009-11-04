
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
#include "operations.hpp"

void setup_io(int *fd, void **map, workload_config_t *config) {
    int res;
    int flags = 0;
    
    if(!config->do_atime)
        flags |= O_NOATIME;
    
    if(config->io_type == iot_mmap) {
        if(config->operation == op_write)
            flags |= O_RDWR;
        else
            flags |= O_RDONLY;
    } else {
        if(config->operation == op_read)
            flags |= O_RDONLY;
        else if(config->operation == op_write)
            flags |= O_WRONLY;
        else
            check("Invalid operation", 1);
    }
        
    if(config->direct_io)
        flags |= O_DIRECT;
    
    if(config->append_only)
        flags |= O_APPEND;

    *fd = open64(config->device, flags);
    check("Error opening device", *fd == -1);

    if(config->io_type == iot_mmap) {
        int prot = 0;
        if(config->operation == op_read)
            prot = PROT_READ;
        else
            prot = PROT_WRITE | PROT_READ;
        *map = mmap(NULL,
                    config->device_length,
                    prot, MAP_SHARED, *fd, 0);
        check("Unable to mmap memory", *map == MAP_FAILED);
    }

    if(config->io_type == iot_paio) {
        aioinit aio_config;
        bzero((void*)&aio_config, sizeof(aio_config));
        aio_config.aio_threads = config->queue_depth;
        aio_config.aio_num = config->queue_depth;
        aio_init(&aio_config);
    }
}

void cleanup_io(int fd, void *map, workload_config_t *config) {
    if(map) {
        check("Unable to unmap memory",
              munmap(map, config->device_length) != 0);
    }
    int res = close(fd);
    check("Could not close the file", res == -1);
}

void* simulation_worker(void *arg) {
    simulation_info_t *info = (simulation_info_t*)arg;
    rnd_gen_t rnd_gen;
    char *buf;

    int res = posix_memalign((void**)&buf,
                             std::max(getpagesize(), info->config->block_size),
                             info->config->block_size);
    check("Error allocating memory", res != 0);

    rnd_gen = init_rnd_gen();
    check("Error initializing random numbers", rnd_gen == NULL);

    char sum = 0;
    while(!(*info->is_done)) {
        long long ops = __sync_fetch_and_add(&(info->ops), 1);
        if(!perform_op(info->fd, info->mmap, buf, ops, rnd_gen, info->config)) {
            *info->is_done = 1;
            goto done;
        }
        // Read from the buffer to make sure there is no optimization
        // shenanigans
	sum += buf[0];
    }

done:
    free_rnd_gen(rnd_gen);
    free(buf);

    return (void*)sum;
}

void print_stats(ticks_t start_time, ticks_t end_time, long long ops, workload_config_t *config) {
    if(config->duration_unit == dut_interactive)
        return;
    float total_secs = ticks_to_secs(end_time - start_time);
    if(config->silent) {
        printf("%d %.2f\n",
               (int)((float)ops / total_secs),
               ((double)ops * config->block_size / 1024 / 1024) / total_secs);
    } else {
        printf("Operations/sec: %d (%.2f MB/sec) - %lld ops in %.2f secs\n",
               (int)((float)ops / total_secs),
               ((double)ops * config->block_size / 1024 / 1024) / total_secs,
               ops, ticks_to_secs(end_time - start_time));
    }
}

long long compute_total_ops(workload_simulation_t *ws) {
    long long ops = 0;
    for(int i = 0; i < ws->config.threads; i++) {
        ops += __sync_fetch_and_add(&(ws->sim_infos[i]->ops), 0);
    }
    return ops;
}


