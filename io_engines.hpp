
#ifndef __IO_ENGINES_HPP__
#define __IO_ENGINES_HPP__

#include "io_engine.hpp"

// Stateful engine
class io_engine_stateful_t : public io_engine_t {
public:
    virtual void perform_read_op(off64_t offset, char *buf);
    virtual void perform_write_op(off64_t offset, char *buf);
};

// Stateless engine
class io_engine_stateless_t : public io_engine_t {
public:
    virtual void perform_read_op(off64_t offset, char *buf);
    virtual void perform_write_op(off64_t offset, char *buf);
};

// PAIO engine
class io_engine_paio_t : public io_engine_t {
public:
    virtual void post_open_setup();
    virtual void perform_read_op(off64_t offset, char *buf);
    virtual void perform_write_op(off64_t offset, char *buf);
};

// mmap engine
class io_engine_mmap_t : public io_engine_t {
public:
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

