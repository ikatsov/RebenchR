
#include <aio.h>
#include <strings.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include "opts.hpp"
#include "utils.hpp"
#include "workload.hpp"

int is_done(off64_t offset, workload_config_t *config)
{
    if(config->workload == wl_rnd)
        return 0;
    if(config->workload == wl_seq && config->operation == op_write && config->direction == opd_forward)
        return 0;
    
    if(config->direction == opd_forward) {
        return offset >= config->offset + config->length;
    } else if(config->direction == opd_backward) {
        return offset < config->device_length - config->offset - config->length;
    }

    return 0;
}

off64_t prepare_offset(long long ops, rnd_gen_t rnd_gen, workload_config_t *config)
{
    off64_t offset = -1;
            
    // Setup the offset
    if(config->workload == wl_rnd) {
        offset = config->offset +
            (get_random(rnd_gen, config->dist, config->length, config->sigma)
             / config->stride * config->stride);
    } else if(config->workload == wl_seq) {
        if(config->direction == opd_forward)
            offset = config->offset + ops * config->stride;
        else if(config->direction == opd_backward) {
            size_t boundary = config->device_length - config->offset;
            boundary = boundary / 512 * 512;
            offset = boundary - config->block_size - ops * config->stride;
        }
    }

    return offset;
}

