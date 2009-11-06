
#include <errno.h>
#include <aio.h>
#include <strings.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include "io_engines.hpp"
#include "workload.hpp"

/**
 * Stateful engine
 **/
void io_engine_stateful_t::perform_read_op(off64_t offset, char *buf) {
    off64_t res = -1;
    
    res = lseek64(fd, offset, SEEK_SET);
    check("Error seeking through device", res == -1);
    
    res = read(fd, buf, config->block_size);
    check("Error reading from device", res == -1);
    check("Attempting to read from the end of the device", res == 0);
}

void io_engine_stateful_t::perform_write_op(off64_t offset, char *buf) {
    off64_t res = -1;
    
    if(!config->append_only) {
        res = lseek64(fd, offset, SEEK_SET);
        check("Error seeking through device", res == -1);
    }

    res = write(fd, buf, config->block_size);
    check("Error writing to device", res == -1 || res != config->block_size);
    
    if(!config->buffered) {
        if(config->do_atime)
            check("Error syncing data", fsync(fd) == -1);
        else
            check("Error syncing data", fdatasync(fd) == -1);
    }
}

/**
 * Stateless engine
 **/
void io_engine_stateless_t::perform_read_op(off64_t offset, char *buf) {
    off64_t res = -1;
    res = pread64(fd, buf, config->block_size, offset);
    check("Error reading from device", res == -1);
    check("Attempting to read from the end of the device", res == 0);
}

void io_engine_stateless_t::perform_write_op(off64_t offset, char *buf) {
    off64_t res = -1;
    res = pwrite64(fd, buf, config->block_size, offset);
    check("Error writing to device", res == -1 || res != config->block_size);
    if(!config->buffered) {
        if(config->do_atime)
            check("Error syncing data", fsync(fd) == -1);
        else
            check("Error syncing data", fdatasync(fd) == -1);
    }
}

/**
 * PAIO engine
 **/
void io_engine_paio_t::post_open_setup() {
    /*
    aioinit aio_config;
    bzero((void*)&aio_config, sizeof(aio_config));
    aio_config.aio_threads = config->queue_depth;
    aio_config.aio_num = config->queue_depth;
    aio_init(&aio_config);
    */
    // This causes crashes for some reason.
}

void io_engine_paio_t::perform_read_op(off64_t offset, char *buf) {
    check("Unused - if you see this, it's a bug in rebench", 1);
}
void io_engine_paio_t::perform_write_op(off64_t offset, char *buf) {
    check("Unused - if you see this, it's a bug in rebench", 1);
}
int io_engine_paio_t::perform_op(char *buf, long long ops, rnd_gen_t rnd_gen) {
    check("Unused - if you see this, it's a bug in rebench", 1);
}

void io_engine_paio_t::perform_read_op(off64_t offset, char *buf, aiocb64 *request) {
    off64_t res = -1;
    
    bzero(request, sizeof(aiocb64));
    request->aio_fildes = fd;
    request->aio_offset = offset;
    request->aio_nbytes = config->block_size;
    request->aio_buf = buf;
    res = aio_read64(request);
    check("aio_read failed", res != 0);
}

void io_engine_paio_t::perform_write_op(off64_t offset, char *buf, aiocb64 *request) {
    off64_t res = -1;
    
    bzero(request, sizeof(aiocb64));
    request->aio_fildes = fd;
    request->aio_offset = offset;
    request->aio_nbytes = config->block_size;
    request->aio_buf = buf;
    res = aio_write64(request);
    check("aio_write failed", res != 0);
}

int io_engine_paio_t::perform_op(char *buf, aiocb64 *request, long long ops, rnd_gen_t rnd_gen) {
    off64_t res;
    off64_t offset = prepare_offset(ops, rnd_gen, config);
    if(::is_done(offset, config)) {
        return 0;
    }

    if(config->duration_unit == dut_interactive) {
        char in;
        // Ask for confirmation before the operation
        printf("%lld: Press enter to perform operation, or 'q' to quit: ", ops);
        in = getchar();
        if(in == EOF || in == 'q') {
            return 0;
        }
    }
    
    // Perform the operation
    if(config->operation == op_read)
        perform_read_op(offset, buf, request);
    else if(config->operation == op_write)
        perform_write_op(offset, buf, request);
    
    return 1;
}

void io_engine_paio_t::run_benchmark() {
    // Create the arrays of requests and buffers
    requests = (aiocb64*)malloc(sizeof(aiocb64) * config->queue_depth);
    
    char *buf;
    int res = posix_memalign((void**)&buf,
                             std::max(getpagesize(), config->block_size),
                             config->block_size * config->queue_depth);
    check("Error allocating memory", res != 0);

    // Initialize random number generator
    rnd_gen_t rnd_gen;
    rnd_gen = init_rnd_gen();
    check("Error initializing random numbers", rnd_gen == NULL);
    
    // Fill up the queue with initial requests
    aiocb64* aio_reqs[config->queue_depth];
    for(int i = 0; i < config->queue_depth; i++) {
        aio_reqs[i] = &requests[i];
        long long _ops = __sync_fetch_and_add(&ops, 1);
        if(!perform_op(buf + config->block_size * i, aio_reqs[i], _ops, rnd_gen)) {
            *is_done = 1;
            goto done;
        }
    }

    // Add more requests as we get results, or quit when done
    while(!(*is_done)) {
        res = aio_suspend64(aio_reqs, config->queue_depth, NULL);
        check("aio_suspend failed", res != 0);

        // Look through the requests
        for(int i = 0; i < config->queue_depth; i++) {
            res = aio_error64(aio_reqs[i]);
            if(res > 0)
                errno = res;
            check("Error completing aio request", res > 0 && res != EINPROGRESS);
            if(res == 0) {
                // Check return value
                res = aio_return64(aio_reqs[i]);
                check("Error reading from device", res == -1);
                
                // Submit another request
                long long _ops = __sync_fetch_and_add(&ops, 1);
                if(!perform_op(buf + config->block_size * i, aio_reqs[i], _ops, rnd_gen)) {
                    *is_done = 1;
                    goto done;
                }
            }
        }
    }

done:
    free_rnd_gen(rnd_gen);
    free(requests);
    free(buf);
}

/**
 * mmap engine
 **/
int io_engine_mmap_t::contribute_open_flags() {
    if(config->operation == op_write)
        return O_RDWR;
    else
        return O_RDONLY;
}

void io_engine_mmap_t::post_open_setup() {
    int prot = 0;
    if(config->operation == op_read)
        prot = PROT_READ;
    else
        prot = PROT_WRITE | PROT_READ;
    map = mmap(NULL,
               config->device_length,
               prot, MAP_SHARED, fd, 0);
    check("Unable to mmap memory", map == MAP_FAILED);
}

void io_engine_mmap_t::pre_close_teardown() {
    check("Unable to unmap memory",
          munmap(map, config->device_length) != 0);
}

void io_engine_mmap_t::perform_read_op(off64_t offset, char *buf) {
    memcpy(buf, (char*)map + offset, config->block_size);
}

void io_engine_mmap_t::perform_write_op(off64_t offset, char *buf) {
    memcpy((char*)map + offset, buf, config->block_size);
    if(!config->buffered) {
        check("Could not flush mmapped memory",
              msync(map, offset + config->block_size, MS_SYNC) != 0);
    }
}

void io_engine_mmap_t::copy_io_state(io_engine_t *io_engine) {
    io_engine_t::copy_io_state(io_engine);
    map = (dynamic_cast<io_engine_mmap_t*>(io_engine))->map;
}
