
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
            else
                printf("Benchmarking...\n\n");
        }
        workload++;

        ws->is_done = 0;
        ws->ops = 0;
        ws->mmap = NULL;
        ws->start_time = get_ticks();
        init_std_dev(&(ws->std_dev));
        io_engine_t *first_engine = NULL;
        for(int i = 0; i < ws->config.threads; i++) {
            io_engine_t *io_engine = make_engine(ws->config.io_type);
            io_engine->config = &ws->config;
            io_engine->is_done = &ws->is_done;
            if(!ws->config.local_fd) {
                if(first_engine == NULL) {
                    setup_io(&ws->config, io_engine);
                } else {
                    io_engine->copy_io_state(first_engine);
                }
            } else {
                setup_io(&ws->config, io_engine);
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
    ticks_t last_ticks_now = get_ticks();

    // stats info
    int n_stats_reports = 0;
    long long min_ops_per_sec = 1000000, max_ops_per_sec = 0;
    unsigned long long last_ops_so_far = 0;
    
    while(!all_done) {
        all_done = true;
        for(wsp_vector::iterator it = workloads->begin(); it != workloads->end(); ++it) {
            workload_simulation_t *ws = *it;
            
            // Compute ops so far
            unsigned long long ops_so_far = 0;
            if((!ws->is_done && ws->config.duration_unit == dut_space) || ws->config.stats_type == st_op_aggregate) {
                ops_so_far = compute_total_ops(ws);
            }
            
            // See if the workload is done
            if(!ws->is_done) {
                if(ws->config.duration_unit == dut_space) {
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
                    // The simulation thread will handle exiting
                    while(!ws->is_done) {
                        usleep(5000);
                    }
                }
            }

            // See if we need to compute aggregate stats
            if(ws->config.stats_type == st_op_aggregate) {
                ticks_t ticks_now = get_ticks();
                unsigned long long ms_passed = ticks_to_ms(ticks_now - last_ticks_now);
                if(ms_passed >= ws->config.aggregation_step) {
                    // Compute current stats
                    unsigned long long ops_this_time = ops_so_far - last_ops_so_far;
                    int ops_per_sec = (float)ops_this_time / ((float)ms_passed / 1000.0f);
                    last_ops_so_far = ops_so_far;

                    if(ops_per_sec < min_ops_per_sec)
                        min_ops_per_sec = ops_per_sec;
                    if(ops_per_sec > max_ops_per_sec)
                        max_ops_per_sec = ops_per_sec;
                    add_to_std_dev(&(ws->std_dev), ops_per_sec);
                    
                    // Reset the counter
                    n_stats_reports++;
                    last_ticks_now = ticks_now;
                }
            }
            
            // If the workload is done, wait for all the threads and grab the time
            if(ws->is_done) {
                for(int i = 0; i < ws->config.threads; i++) {
                    check("Error joining thread",
                          pthread_join(ws->threads[i], NULL) != 0);
                }
                ws->end_time = get_ticks();
                if(ws->config.stats_type == st_op_aggregate) {
                    ws->min_ops_per_sec = min_ops_per_sec;
                    ws->max_ops_per_sec = max_ops_per_sec;
                }
            }
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
        float min_op_time_in_ms = 1000000.0f, max_op_time_in_ms = 0.0f, op_total_ms = 0.0f;
        
        workload_simulation_t *ws = *it;
        float total_variance = 0.0f;
        for(int i = 0; i < ws->config.threads; i++) {
            // Compute stats
            if(ws->engines[i]->min_op_time_in_ms < min_op_time_in_ms)
                min_op_time_in_ms = ws->engines[i]->min_op_time_in_ms;
            if(ws->engines[i]->max_op_time_in_ms > max_op_time_in_ms)
                max_op_time_in_ms = ws->engines[i]->max_op_time_in_ms;
            op_total_ms += ws->engines[i]->op_total_ms;
            // Looks like variances can be pooled, but we should check
            // with a stats book
            total_variance += get_variance(&(ws->engines[i]->std_dev));

            // Clean up local fds
            if(ws->config.local_fd)
                cleanup_io(&ws->config, ws->engines[i]);
        }
        total_variance /= (float)ws->config.threads;
        float std_dev = sqrt(total_variance);
        ws->ops = compute_total_ops(ws);
        
        if(!ws->config.local_fd)
            cleanup_io(&ws->config, ws->engines[0]);

        // print results
        if(it != workloads->begin())
            printf("---\n");
        print_status(ws->config.device_length, &ws->config);
        print_stats(ws->start_time, ws->end_time, ws->ops,
                    &ws->config,
                    min_op_time_in_ms, max_op_time_in_ms, op_total_ms, std_dev,
                    ws->min_ops_per_sec, ws->max_ops_per_sec,
                    sqrt(get_variance(&(ws->std_dev))));

        // clean up
        for(int i = 0; i < ws->engines.size(); i++)
            delete ws->engines[i];
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
