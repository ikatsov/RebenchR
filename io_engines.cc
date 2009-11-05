
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
    aioinit aio_config;
    bzero((void*)&aio_config, sizeof(aio_config));
    aio_config.aio_threads = config->queue_depth;
    aio_config.aio_num = config->queue_depth;
    aio_init(&aio_config);
}

void io_engine_paio_t::perform_read_op(off64_t offset, char *buf) {
    off64_t res = -1;
    
    // TODO: implement queue depth
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
    
    check("Error reading from device", res == -1);
    check("Attempting to read from the end of the device", res == 0);
}

void io_engine_paio_t::perform_write_op(off64_t offset, char *buf) {
    off64_t res = -1;
    
    // TODO: implement queue depth
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

    check("Error writing to device", res == -1 || res != config->block_size);
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
