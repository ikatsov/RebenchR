
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <aio.h>
#include <string.h>
#include <sys/mman.h>
#include "opts.hpp"
#include "utils.hpp"
#include "operations.hpp"

int is_done(off64_t offset, off64_t length, workload_config_t *config) {
    if(config->workload == wl_rnd)
        return 0;
    return (offset >= length || offset < 0)
        && !(config->workload == wl_seq && config->operation == op_write && config->direction == opd_forward);
}

off64_t prepare_offset(off64_t length, unsigned int ops, rnd_gen_t rnd_gen,
                       workload_config_t *config) {
    off64_t offset = -1;
            
    // Setup the offset
    if(config->workload == wl_rnd) {
        offset = (get_random(rnd_gen, config->dist, length, config->sigma) /
                  config->stride * config->stride);
    } else if(config->workload == wl_seq) {
        if(config->direction == opd_forward)
            offset = ops * config->stride;
        else if(config->direction == opd_backward) {
            size_t boundary = length;
            boundary = boundary / 512 * 512;
            offset = boundary - config->block_size - ops * config->stride;
        }
    }

    return offset;
}

void perform_read_op(int fd, void *mmap, off64_t offset, char *buf,
                     workload_config_t *config) {
    off64_t res = -1;
    
    // IO type
    if(config->io_type == iot_stateful) {
        res = lseek64(fd, offset, SEEK_SET);
        check("Error seeking through device", res == -1);
        res = read(fd, buf, config->block_size);
    } else if(config->io_type == iot_stateless)
        res = pread64(fd, buf, config->block_size, offset);
    else if(config->io_type == iot_aio) {
        aiocb64 read_aio;
        bzero(&read_aio, sizeof(read_aio));
        read_aio.aio_fildes = fd;
        read_aio.aio_offset = offset;
        read_aio.aio_nbytes = config->block_size;
        read_aio.aio_buf = buf;
        res = aio_read64(&read_aio);
        check("aio_read failed", res != 0);

        // Wait for it to complete
        aiocb64* aio_reqs[1];
        aio_reqs[0] = &read_aio;
        res = aio_suspend64(aio_reqs, 1, NULL);
        check("aio_suspend failed", res != 0);

        // Find out how much we've read
        res = aio_return64(&read_aio);
    } else if(config->io_type == iot_mmap) {
        memcpy(buf, (char*)mmap + offset, config->block_size);
        res = config->block_size;
    }
                
    check("Error reading from device", res == -1);
    check("Attempting to read from the end of the device", res == 0);
}

void perform_write_op(int fd, void *mmap, off64_t offset, char *buf,
                      workload_config_t *config) {
    off64_t res = -1;
    
    // IO type
    if(config->io_type == iot_stateful) {
        if(!config->append_only) {
            res = lseek64(fd, offset, SEEK_SET);
            check("Error seeking through device", res == -1);
        }
        res = write(fd, buf, config->block_size);
    } else if(config->io_type == iot_stateless) {
        res = pwrite64(fd, buf, config->block_size, offset);
    }
    else if(config->io_type == iot_aio) {
        aiocb64 write_aio;
        bzero(&write_aio, sizeof(write_aio));
        write_aio.aio_fildes = fd;
        write_aio.aio_offset = offset;
        write_aio.aio_nbytes = config->block_size;
        write_aio.aio_buf = buf;
        res = aio_write64(&write_aio);
        check("aio_write failed", res != 0);

        // Wait for it to complete
        aiocb64* aio_reqs[1];
        aio_reqs[0] = &write_aio;
        res = aio_suspend64(aio_reqs, 1, NULL);
        check("aio_suspend failed", res != 0);

        // Find out how much we've written
        res = aio_return64(&write_aio);
    } else if(config->io_type == iot_mmap) {
        memcpy((char*)mmap + offset, buf, config->block_size);
        res = config->block_size;
    }

    if(config->io_type != iot_aio) {
        if(!config->buffered) {
            if(config->io_type == iot_mmap) {
                check("Could not flush mmapped memory",
                      msync(mmap, offset + config->block_size, MS_SYNC) != 0);
            } else {
                if(config->do_atime)
                    check("Error syncing data", fsync(fd) == -1);
                else
                    check("Error syncing data", fdatasync(fd) == -1);
            }
        }
    }
                
    check("Error writing to device", res == -1 || res != config->block_size);
}

int perform_op(int fd, void *mmap, char *buf, off64_t length, unsigned int ops, rnd_gen_t rnd_gen,
               workload_config_t *config) {
    off64_t res;
    off64_t offset = prepare_offset(length, ops, rnd_gen, config);
    if(is_done(offset, length, config))
        return 0;

    // Perform the operation
    if(config->operation == op_read)
        perform_read_op(fd, mmap, offset, buf, config);
    else if(config->operation == op_write)
        perform_write_op(fd, mmap, offset, buf, config);
    
    return 1;
}

