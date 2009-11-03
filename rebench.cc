
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>
#include <strings.h>
#include <algorithm>
#include <sys/mman.h>
#include <vector>
#include "opts.hpp"
#include "utils.hpp"
#include "operations.hpp"
#include "simulation.hpp"

int main(int argc, char *argv[])
{
    int i = 0;
    wsp_vector workloads;

    // Parse the workloads
    if(argc < 2) {
        char buf[2048];
        char delims[] = " \t\n";
        while(fgets(buf, 2048, stdin)) {
            std::vector<char*> args;
            char *tok = strtok(buf, delims);
            while(tok) {
                args.push_back(tok);
                tok = strtok(NULL, delims);
            }
            if(!args.empty()) {
                args.insert(args.begin(), argv[0]);
                workload_simulation_t *ws = new workload_simulation_t();
                parse_options(args.size(), &args[0], &ws->config);
                if(ws->config.duration_unit == dut_interactive) {
                    check("Cannot run in interactive mode with stdin workloads", 1);
                }
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

    // Drop caches if necessary
    for(wsp_vector::iterator it = workloads.begin(); it != workloads.end(); ++it) {
        workload_simulation_t *ws = *it;
        if(ws->config.drop_caches) {
            drop_caches(ws->config.device);
        }
    }

    // Start the simulations
    int workload = 1;
    for(wsp_vector::iterator it = workloads.begin(); it != workloads.end(); ++it) {
        workload_simulation_t *ws = *it;

        if(!ws->config.silent) {
            if(workloads.size() > 1) {
                printf("Starting workload %d...\n", workload);
                if(workload == workloads.size())
                    printf("\n");
            }
            else
                printf("Benchmarking...\n\n");
        }
        workload++;

        ws->is_done = 0;
        ws->ops = 0;
        ws->mmap = NULL;
        if(!ws->config.local_fd) {
            // FD is shared for the workload
            setup_io(&ws->fd, &ws->mmap, &ws->config);
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
                sim_info->mmap = ws->mmap;
            } else {
                setup_io(&(sim_info->fd), &(sim_info->mmap), &ws->config);
            }
            pthread_t thread;
            check("Error creating thread",
                  pthread_create(&thread, NULL, &simulation_worker, (void*)sim_info) != 0);
            ws->sim_infos.push_back(sim_info);
            ws->threads.push_back(thread);
        }
    }

    // Stop the simulations
    bool all_done = false;
    int total_slept = 0;
    while(!all_done) {
        all_done = true;
        for(wsp_vector::iterator it = workloads.begin(); it != workloads.end(); ++it) {
            workload_simulation_t *ws = *it;
            // See if the workload is done
            if(!ws->is_done) {
                if(ws->config.duration_unit == dut_space) {
                    long long total_bytes = compute_total_ops(ws) * ws->config.block_size;
                    if(total_bytes >= ws->config.duration) {
                        ws->is_done = 1;
                    } else {
                        all_done = false;
                    }
                } else if(ws->config.duration_unit == dut_time) {
                    if(total_slept / 200.0f >= ws->config.duration) {
                        ws->is_done = 1;
                    } else {
                        all_done = false;
                    }
                } else {
                    // The simulation thread will handle exiting
                    while(!ws->is_done) {
                        usleep(5000);
                    }
                }
            }
            // If the workload is done, wait for all the threads and grab the time
            if(ws->is_done) {
                for(i = 0; i < ws->config.threads; i++) {
                    check("Error joining thread",
                          pthread_join(ws->threads[i], NULL) != 0);
                }
                ws->end_time = get_ticks();
            }
        }
        
        if(!all_done) {
            usleep(5000);
            total_slept++;
        }
    }
    
    // Compute the stats
    for(wsp_vector::iterator it = workloads.begin(); it != workloads.end(); ++it) {
        workload_simulation_t *ws = *it;
        for(i = 0; i < ws->config.threads; i++) {
            if(ws->config.local_fd)
                cleanup_io(ws->sim_infos[i]->fd, ws->sim_infos[i]->mmap, &ws->config);
        }
        ws->ops = compute_total_ops(ws);
        
        if(!ws->config.local_fd)
            cleanup_io(ws->fd, ws->mmap, &ws->config);

        // print results
        if(it != workloads.begin())
            printf("---\n");
        print_status(ws->config.device_length, &ws->config);
        print_stats(ws->start_time, ws->end_time, ws->ops, &ws->config);

        // clean up
        for(i = 0; i < ws->sim_infos.size(); i++)
            delete ws->sim_infos[i];
        delete ws;
    }
}
