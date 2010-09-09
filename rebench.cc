
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
#include <math.h>
#include "opts.hpp"
#include "utils.hpp"
#include "io_engine.hpp"
#include "simulation.hpp"

void parse_workloads(int argc, char *argv[], wsp_vector *workloads) {
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
                workloads->push_back(ws);
            }
        }
    } else {
        workload_simulation_t *ws = new workload_simulation_t();
        parse_options(argc, argv, &ws->config);
        workloads->push_back(ws);
    }

    if(workloads->empty())
        usage(argv[0]);
}

void drop_workload_caches(wsp_vector *workloads) {
    // Drop caches if necessary
    for(wsp_vector::iterator it = workloads->begin(); it != workloads->end(); ++it) {
        workload_simulation_t *ws = *it;
        if(ws->config.drop_caches) {
            drop_caches(ws->config.device);
        }
    }
}

void start_simulations(wsp_vector *workloads) {
    // Start the simulations
    int workload = 1;
    
    for(wsp_vector::iterator it = workloads->begin(); it != workloads->end(); ++it) {
        workload_simulation_t *ws = *it;

        if(!ws->config.silent) {
            if(workloads->size() > 1) {
                printf("Starting workload %d...\n", workload);
                if(workload == workloads->size())
                    printf("\n");
            }
            else {
                printf("Benchmarking...\n\n");
            }
        }
        workload++;

        ws->is_done = 0;
        ws->ops = 0;
        ws->mmap = NULL;
        ws->start_time = get_ticks();
        init_std_dev(&(ws->std_dev));
        io_engine_t *first_engine = NULL;
        pthread_mutex_init(&ws->latency_mutex, NULL);
        for(int i = 0; i < ws->config.threads; i++) {
            io_engine_t *io_engine = make_engine(ws->config.io_type, &ws->latencies, &ws->latency_mutex);
            io_engine->config = &ws->config;
            io_engine->is_done = &ws->is_done;
            if(!ws->config.local_fd) {
                if(first_engine == NULL) {
                    setup_io(&ws->config, ws, io_engine);
                } else {
                    io_engine->copy_io_state(first_engine);
                }
            } else {
                setup_io(&ws->config, ws, io_engine);
            }
            pthread_t thread;
            check("Error creating thread",
                  pthread_create(&thread, NULL, &simulation_worker, (void*)io_engine) != 0);
            ws->engines.push_back(io_engine);
            ws->threads.push_back(thread);
            if(first_engine == NULL)
                first_engine = io_engine;
        }
    }
}

void stop_simulations(wsp_vector *workloads) {
    // Stop the simulations
    bool all_done = false;
    int total_slept = 0;
    ticks_t last_ticks_now = get_ticks(), ticks_now = 0;
    workload_config_t *config = &((*(workloads->begin()))->config);

    while(!all_done) {
        all_done = true;
        for(wsp_vector::iterator it = workloads->begin(); it != workloads->end(); ++it) {
            workload_simulation_t *ws = *it;
            
            // Compute ops so far
            long long ops_so_far = -1;
            
            // See if the workload is done
            if(!ws->is_done) {
                if(ws->config.duration_unit == dut_space) {
                    ops_so_far = compute_total_ops(ws);
                    long long total_bytes = ops_so_far * ws->config.block_size;
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
                    // We're interactive, the simulation thread will
                    // handle exiting
                    while(!ws->is_done) {
                        usleep(5000);
                    }
                }
            }

            // Compute stats
            if(ws->config.sample_step == 0) {
                // If sample step is zero, we're dealing with
                // latencies for any given op instead of ops per
                // second.
                // TODO: implement
                for(int i = 0; i < ws->latencies.size(); i++) {
                    unsigned long long latency = ws->latencies[i];
                    ws->sum_latency += latency;
                    add_to_std_dev(&(ws->std_dev), latency);
                    if(latency < ws->min_latency)
                        ws->min_latency = latency;
                    if(latency > ws->max_latency)
                        ws->max_latency = latency;
                }
                ws->latencies.clear();
            } else {
                ticks_now = get_ticks();
                unsigned long long ms_passed = ticks_to_ms(ticks_now - last_ticks_now);
                if(ms_passed >= ws->config.sample_step) {
                    // Compute current stats
                    if(ops_so_far == -1) {
                        ops_so_far = compute_total_ops(ws);
                    }
                    unsigned long long ops_this_time = ops_so_far - ws->last_ops_so_far;
                    int ops_per_sec = (float)ops_this_time / ((float)ms_passed / 1000.0f);
                    ws->last_ops_so_far = ops_so_far;

                    if(ops_per_sec < ws->min_ops_per_sec)
                        ws->min_ops_per_sec = ops_per_sec;
                    if(ops_per_sec > ws->max_ops_per_sec)
                        ws->max_ops_per_sec = ops_per_sec;
                    add_to_std_dev(&(ws->std_dev), ops_per_sec);

                    if(ws->output_fd != -1) {
                        char databuf[255];
                        int outcount = snprintf(databuf, 255, "%d\n", ops_per_sec);
                        int res = write(ws->output_fd, databuf, outcount);
                        check("Could not record output data", res != outcount);
                    }
                }
            }
            
            // If the workload is done, wait for all the threads and grab the time
            if(ws->is_done) {
                for(int i = 0; i < ws->config.threads; i++) {
                    check("Error joining thread",
                          pthread_join(ws->threads[i], NULL) != 0);
                }
                ws->end_time = get_ticks();
            }
        }
        
                    
        // Reset the ticks
        unsigned long long ms_passed = ticks_to_ms(ticks_now - last_ticks_now);
        if(ms_passed >= config->sample_step) {
            last_ticks_now = ticks_now;
        }
                
        if(!all_done) {
            usleep(5000);
            total_slept++;
        }
    }
}

void compute_stats(wsp_vector *workloads) {
    // Compute the stats
    for(wsp_vector::iterator it = workloads->begin(); it != workloads->end(); ++it) {
        workload_simulation_t *ws = *it;
        for(int i = 0; i < ws->config.threads; i++) {
            // Clean up local fds
            if(ws->config.local_fd)
                cleanup_io(&ws->config, ws, ws->engines[i]);
        }
        ws->ops = compute_total_ops(ws);
        
        if(!ws->config.local_fd)
            cleanup_io(&ws->config, ws, ws->engines[0]);

        // print results
        if(it != workloads->begin())
            printf("---\n");
        print_status(ws->config.device_length, &ws->config);
        print_stats(ws->start_time, ws->end_time, ws->ops,
                    &ws->config,
                    ws->min_ops_per_sec, ws->max_ops_per_sec,
                    sqrt(get_variance(&(ws->std_dev))),
                    ws->sum_latency, ws->min_latency, ws->max_latency);

        // clean up
        for(int i = 0; i < ws->engines.size(); i++)
            delete ws->engines[i];
        pthread_mutex_destroy(&ws->latency_mutex);
        delete ws;
    }
}

int main(int argc, char *argv[])
{
    wsp_vector workloads;

    parse_workloads(argc, argv, &workloads);
    drop_workload_caches(&workloads);
    start_simulations(&workloads);
    stop_simulations(&workloads);
    compute_stats(&workloads);
}
