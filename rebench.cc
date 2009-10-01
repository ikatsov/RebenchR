
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>
#include <strings.h>
#include <vector>
#include <algorithm>
#include <sys/mman.h>
#include "opts.hpp"
#include "utils.hpp"
#include "operations.hpp"

using namespace std;

void setup_io(int *fd, off64_t *length, void **map, workload_config_t *config) {
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

    *length = lseek64(*fd, 0, SEEK_END);
    check("Error computing device size", *length == -1);

    if(config->io_type == iot_mmap) {
        int prot = 0;
        if(config->operation == op_read)
            prot = PROT_READ;
        else
            prot = PROT_WRITE | PROT_READ;
        *map = mmap(NULL, *length, prot, MAP_SHARED, *fd, 0);
        check("Unable to mmap memory", *map == MAP_FAILED);
    }
}

void cleanup_io(int fd, void *map, off64_t length) {
    if(map) {
        check("Unable to unmap memory",
              munmap(map, length) != 0);
    }
    close(fd);
}

// Describes each thread in a workload
struct simulation_info_t {
    int *is_done;
    int ops;
    int fd;
    off64_t length;
    workload_config_t *config;
    void *mmap;
};

// Describes each workload simulation
struct workload_simulation_t {
    vector<simulation_info_t*> sim_infos;
    vector<pthread_t> threads;
    workload_config_t config;
    int is_done;
    int fd;
    off64_t length;
    ticks_t start_time, end_time;
    int ops;
    void *mmap;
};
typedef vector<workload_simulation_t*> wsp_vector;

void* run_simulation(void *arg) {
    simulation_info_t *info = (simulation_info_t*)arg;
    rnd_gen_t rnd_gen;
    unsigned int ops = 0;
    char *buf;

    int res = posix_memalign((void**)&buf, HARDWARE_BLOCK_SIZE, info->config->block_size);
    check("Error allocating memory", res != 0);

    rnd_gen = init_rnd_gen();
    check("Error initializing random numbers", rnd_gen == NULL);

    while(!(*info->is_done)) {
        if(!perform_op(info->fd, info->mmap, buf, info->length, ops, rnd_gen, info->config))
            goto done;

        ops++;
    }

done:
    free_rnd_gen(rnd_gen);
    free(buf);
    info->ops = ops;

    return 0;
}

int workloads_cmp(workload_simulation_t *x, workload_simulation_t *y) {
    return x->config.duration < y->config.duration;
}

void print_stats(ticks_t start_time, ticks_t end_time, int ops, workload_config_t *config) {
    float total_secs = ticks_to_secs(end_time - start_time);
    printf("Operations/sec: %d (%.2f MB/sec)\n",
           (int)(ops / total_secs),
           ((double)ops * config->block_size / 1024 / 1024) / total_secs);
}

int main(int argc, char *argv[])
{
    int i = 0;
    wsp_vector workloads;

    // Parse the workloads
    if(argc < 2) {
        char buf[2048];
        char delims[] = " \t\n";
        while(fgets(buf, 2048, stdin)) {
            vector<char*> args;
            char *tok = strtok(buf, delims);
            while(tok) {
                args.push_back(tok);
                tok = strtok(NULL, delims);
            }
            if(!args.empty()) {
                args.insert(args.begin(), argv[0]);
                workload_simulation_t *ws = new workload_simulation_t();
                parse_options(args.size(), &args[0], &ws->config);
                workloads.push_back(ws);
            }
        }
    } else {
        workload_simulation_t *ws = new workload_simulation_t();
        parse_options(argc, argv, &ws->config);
        workloads.push_back(ws);
    }

    if(workloads.empty())
        usage(argv[0]);

    // Sort the workloads based on duration
    sort(workloads.begin(), workloads.end(), workloads_cmp);

    // Start the simulations
    for(wsp_vector::iterator it = workloads.begin(); it != workloads.end(); ++it) {
        workload_simulation_t *ws = *it;
        ws->is_done = 0;
        ws->ops = 0;
        ws->mmap = NULL;
        if(!ws->config.local_fd) {
            // FD is shared for the workload
            setup_io(&ws->fd, &ws->length, &ws->mmap, &ws->config);
        }
        ws->start_time = get_ticks();
        for(i = 0; i < ws->config.threads; i++) {
            simulation_info_t *sim_info = new simulation_info_t();
            bzero(sim_info, sizeof(sim_info));
            sim_info->is_done = &ws->is_done;
            sim_info->config = &ws->config;
            sim_info->mmap = NULL;
            if(!ws->config.local_fd) {
                sim_info->fd = ws->fd;
                sim_info->length = ws->length;
                sim_info->mmap = ws->mmap;
            } else {
                setup_io(&(sim_info->fd), &(sim_info->length), &(sim_info->mmap), &ws->config);
            }
            pthread_t thread;
            check("Error creating thread",
                  pthread_create(&thread, NULL, &run_simulation, (void*)sim_info) != 0);
            ws->sim_infos.push_back(sim_info);
            ws->threads.push_back(thread);
        }
    }

    printf("Benchmarking...\n\n");

    // Stop the simulations
    int last_duration = 0;
    for(wsp_vector::iterator it = workloads.begin(); it != workloads.end(); ++it) {
        workload_simulation_t *ws = *it;
        sleep(ws->config.duration - last_duration);
        last_duration = ws->config.duration;
        
        ws->is_done = 1;
        
        for(i = 0; i < ws->config.threads; i++) {
            check("Error joining thread",
                  pthread_join(ws->threads[i], NULL) != 0);
            if(ws->config.local_fd)
                cleanup_io(ws->sim_infos[i]->fd, ws->sim_infos[i]->mmap, ws->sim_infos[i]->length);
            ws->ops += ws->sim_infos[i]->ops;
        }
        ws->end_time = get_ticks();
        
        if(!ws->config.local_fd)
            cleanup_io(ws->fd, ws->mmap, ws->length);

        // print results
        if(it != workloads.begin())
            printf("---\n");
        print_status(ws->sim_infos[0]->length, &ws->config);
        print_stats(ws->start_time, ws->end_time, ws->ops, &ws->config);

        // clean up
        for(i = 0; i < ws->sim_infos.size(); i++)
            delete ws->sim_infos[i];
        delete ws;
    }
}
