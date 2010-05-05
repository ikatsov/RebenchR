
#ifndef __IO_ENGINE_HPP__
#define __IO_ENGINE_HPP__

#include "simulation.hpp"

#define DEFAULT_MIN_OP_TIME_IN_MS 1000000.0f

class io_engine_t {
public:
    io_engine_t()
        : config(NULL), fd(0), is_done(NULL), ops(0),
          min_op_time_in_ms(DEFAULT_MIN_OP_TIME_IN_MS),
          max_op_time_in_ms(0.0f), op_total_ms(0.0f),
          mk(0.0f), qk(0.0f)
        {}
    
    virtual int contribute_open_flags();
    virtual void post_open_setup();
    virtual void pre_close_teardown();

    virtual void run_benchmark();

    virtual int perform_op(char *buf, long long ops, rnd_gen_t rnd_gen);
    
    virtual void perform_read_op(off64_t offset, char *buf) = 0;
    virtual void perform_write_op(off64_t offset, char *buf) = 0;

    virtual void copy_io_state(io_engine_t *io_engine);
    
public:
    int fd;
    workload_config_t *config;
    int *is_done;
    long ops;

    // Basic stats
    float min_op_time_in_ms, max_op_time_in_ms, op_total_ms;
    
    // Standard deviation in linear time (see http://www.cs.berkeley.edu/~mhoemmen/cs194/Tutorials/variance.pdf)
    float mk, qk;
};

io_engine_t* make_engine(io_type_t engine_type);

#endif // __IO_ENGINE_HPP__

