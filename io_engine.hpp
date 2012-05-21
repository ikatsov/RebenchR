
#ifndef __IO_ENGINE_HPP__
#define __IO_ENGINE_HPP__

#include "simulation.hpp"
#include "utils.hpp"

#define DEFAULT_MIN_OP_TIME_IN_MS 1000000.0f

class io_engine_t {
public:
    io_engine_t(std::vector<latency_t> *_latencies, pthread_mutex_t *_latency_mutex)
        : config(NULL), fd(0), is_done(NULL), ops(0),
          latencies(_latencies), latency_mutex(_latency_mutex)
        {}
    
    virtual int contribute_open_flags();
    virtual void post_open_setup();
    virtual void pre_close_teardown();

    virtual void run_benchmark();

    virtual int perform_op(char *buf, long long ops, rnd_gen_t rnd_gen);
    
    virtual void perform_read_op(off64_t offset, char *buf) = 0;
    virtual void perform_write_op(off64_t offset, char *buf) = 0;
    virtual void perform_trim_op(off64_t offset);

    virtual void copy_io_state(io_engine_t *io_engine);

protected:
    void push_latency(latency_t latency);
    
public:
    int fd;
    workload_config_t *config;
    int *is_done;
    long ops;
    
    std::vector<latency_t> *latencies;
    pthread_mutex_t *latency_mutex;
};

io_engine_t* make_engine(io_type_t engine_type, std::vector<latency_t> *_latencies, pthread_mutex_t *_latency_mutex);

#endif // __IO_ENGINE_HPP__

