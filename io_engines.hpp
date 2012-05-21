
#ifndef __IO_ENGINES_HPP__
#define __IO_ENGINES_HPP__

#include <libaio.h>
#include "io_engine.hpp"

// Stateful engine
class io_engine_stateful_t : public io_engine_t {
public:
    io_engine_stateful_t(std::vector<latency_t> *_latencies, pthread_mutex_t *_latency_mutex)
        : io_engine_t(_latencies, _latency_mutex)
        {}
    virtual void perform_read_op(off64_t offset, char *buf);
    virtual void perform_write_op(off64_t offset, char *buf);
    virtual void perform_trim_op(off64_t offset);
};

// Stateless engine
class io_engine_stateless_t : public io_engine_t {
public:
    io_engine_stateless_t(std::vector<latency_t> *_latencies, pthread_mutex_t *_latency_mutex)
        : io_engine_t(_latencies, _latency_mutex)
        {}
    virtual void perform_read_op(off64_t offset, char *buf);
    virtual void perform_write_op(off64_t offset, char *buf);
};

// PAIO engine
class io_engine_paio_t : public io_engine_t {
public:
    io_engine_paio_t(std::vector<latency_t> *_latencies, pthread_mutex_t *_latency_mutex)
        : io_engine_t(_latencies, _latency_mutex)
        {}
    virtual void post_open_setup();
    virtual void perform_read_op(off64_t offset, char *buf);
    virtual void perform_write_op(off64_t offset, char *buf);
    virtual int perform_op(char *buf, long long ops, rnd_gen_t rnd_gen);
    
    virtual void run_benchmark();

    void perform_read_op(off64_t offset, char *buf, aiocb64 *request);
    void perform_write_op(off64_t offset, char *buf, aiocb64 *request);
    int perform_op(char *buf, aiocb64 *request, long long ops, rnd_gen_t rnd_gen);

private:
    void set_timestamp(aiocb64 *request);
    aiocb64 *requests;
};

// PAIO engine
class io_engine_naio_t : public io_engine_t {
public:
    io_engine_naio_t(std::vector<latency_t> *_latencies, pthread_mutex_t *_latency_mutex)
        : io_engine_t(_latencies, _latency_mutex)
        {}
    virtual void perform_read_op(off64_t offset, char *buf);
    virtual void perform_write_op(off64_t offset, char *buf);
    virtual int perform_op(char *buf, long long ops, rnd_gen_t rnd_gen);
    
    virtual void run_benchmark();

    void perform_read_op(off64_t offset, char *buf, iocb *request);
    void perform_write_op(off64_t offset, char *buf, iocb *request);
    int perform_op(char *buf, iocb *request, long long ops, rnd_gen_t rnd_gen);

private:
    void set_timestamp(iocb *request);
    io_context_t ctx_id;
    iocb *requests;
    int notification_fd;
};

// mmap engine
class io_engine_mmap_t : public io_engine_t {
public:
    io_engine_mmap_t(std::vector<latency_t> *_latencies, pthread_mutex_t *_latency_mutex)
        : io_engine_t(_latencies, _latency_mutex)
        {}
    virtual int contribute_open_flags();
    virtual void post_open_setup();
    virtual void pre_close_teardown();

    virtual void perform_read_op(off64_t offset, char *buf);
    virtual void perform_write_op(off64_t offset, char *buf);

    virtual void copy_io_state(io_engine_t *io_engine);
    
private:
    void *map;
};


#endif // __IO_ENGINES_HPP__

