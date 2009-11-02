
#ifndef __OPTS_HPP__
#define __OPTS_HPP__

// Some constants
extern int HARDWARE_BLOCK_SIZE;

// Workload related enums
enum workload_t {
    wl_seq,
    wl_rnd
};
enum io_type_t {
    iot_stateful,
    iot_stateless,
    iot_aio,
    iot_mmap
};
enum op_direction_t {
    opd_forward,
    opd_backward
};
enum operation_t {
    op_read,
    op_write
};
enum rnd_dist_t {
    rdt_uniform,
    rdt_normal,
    rdt_power
};
enum duration_unit_t {
    dut_time,
    dut_space,
    dut_interactive
};

// Workload config
#define DEVICE_NAME_LENGTH 512
struct workload_config_t {
    int threads;
    int block_size;
    long long duration;
    duration_unit_t duration_unit;
    int stride;
    char device[DEVICE_NAME_LENGTH];
    off64_t offset;
    off64_t length;
    off64_t device_length;
    int direct_io;
    int local_fd;
    int buffered;
    int do_atime;
    int append_only;
    workload_t workload;
    io_type_t io_type;
    op_direction_t direction;
    operation_t operation;
    rnd_dist_t dist;
    int sigma;
    int silent;    
};

// Operations
void check(const char *str, int error);
void usage(const char *name);
void parse_options(int argc, char *argv[], workload_config_t *config);
void print_status(off64_t length, workload_config_t *config);

#endif // __OPTS_HPP__

