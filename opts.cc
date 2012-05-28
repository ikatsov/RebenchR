
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <string.h>
#include <algorithm>
#include "opts.hpp"
#include "utils.hpp"

const int OUTPUT_FLAG = 1024;

int HARDWARE_BLOCK_SIZE = 512;

void print_size(off64_t size);

void init_workload_config(workload_config_t *config) {
    bzero(config, sizeof(*config));
    config->silent = 0;
    config->threads = 1;
    config->block_size = HARDWARE_BLOCK_SIZE;
    config->duration = 10;
    config->duration_unit = dut_time;
    config->stride = HARDWARE_BLOCK_SIZE;
    config->device[0] = NULL;
    config->output_file[0] = NULL;
    config->offset = 0;
    config->length = 0;
    config->direct_io = 1;
    config->local_fd = 0;
    config->buffered = 0;
    config->do_atime = 0;
    config->append_only = 0;
    config->workload = wl_rnd;
    config->io_type = iot_stateful;
    config->queue_depth = 1;
    config->direction = opd_forward;
    config->operation = op_read;
    config->dist = rdt_uniform;
    config->sigma = -1;
    config->sample_step = 1000;
    config->pause_interval = 0;
    config->drop_caches = 0;
    config->use_eventfd = 0;    
}

void usage(const char *name) {
    printf("Usage:\n");
    printf("\t%s [OPTIONS] DEVICE\n", name);

    printf("\n\tAlternatively, if no arguments are passed on the command line, %s will read\n"\
           "\tstandard input and read its arguments from there. Multiple lines may be used\n"\
           "\tto specify multiple concurrent workloads.\n", name);
    
    printf("\nArguments:\n");
    printf("\tDEVICE - device or file name to perform operations on.\n");
    
    printf("\nOptions:\n");
    printf("\t-d, --duration\n\t\tDuration of the benchmark in seconds.\n");
    printf("\t\tDuration can be specified in KB, MB, and GB (add 'k', 'm', or 'g'\n");
    printf("\t\tto the end of the number. The benchmark will stop when that much\n");
    printf("\t\tdata has been processed. If the number is followed by '%%',\n");
    printf("\t\tthe size will be that percentage of the device. If duration is\n");
    printf("\t\tset to 'i', rebench will run commands interactively (useful for\n");
    printf("\t\tdebugging with btrace, and only available in single-threaded mode).\n");
    
    printf("\t-c, --threads\n\t\tNumbers of threads used to run the benchmark.\n");
    printf("\t-b, --block_size\n\t\tSize of blocks in bytes to use for IO operations.\n");
    printf("\t\tBlock size can also be specified in units other than bytes by appending\n");
    printf("\t\t'k', 'm', 'g', or '%%'.\n");

    printf("\t-s, --stride\n\t\tSize of stride in bytes (only applies to sequential workloads).\n");
    printf("\t\tStride can also be specified in units other than bytes by appending\n");
    printf("\t\t'k', 'm', 'g', or '%%'.\n");
    
    printf("\t-w, --workload\n\t\tDescription of a workload to perform.\n");
    printf("\t\tValid options are 'rnd' for a random load, and 'seq' for a sequential load.\n");

    printf("\t-t, --type\n\t\tType of IO calls to use.\n");

    printf("\t\tValid options are 'stateful' for read/write IO,\n" \
           "\t\t'stateless' for pread/pwrite type of IO, 'mmap' for\n" \
           "\t\tmemory mapping, 'paio' for POSIX asynchronous IO,\n" \
           "\t\tand 'naio' for native OS asynchronous IO.\n");
    
    printf("\t-q, --queue-depth\n\t\tThe number of simultaneous AIO calls.\n");
    printf("\t\tValid only during 'paio', and 'naio' type of runs.\n");
    
    printf("\t--eventfd\n\t\tUse eventfd for aio completion notification.\n");
    printf("\t\tValid only during 'naio' type of runs. Useful for measuring eventfd overhead.\n");
    
    printf("\t-r, --direction\n\t\tDirection in which the operations are performed.\n");
    printf("\t\tValid options are 'formward' and 'backward'.\n" \
           "\t\tThis option is only applicable to sequential workloads.\n");
    
    printf("\t-o, --operation\n\t\tThe operation to be performed.\n");
    printf("\t\tValid options are 'read', 'write', and 'trim'.\n");
    printf("\t\tNote that trim is only available on SSD devices\n");
    printf("\t\tvia a stateful interface.\n");
    
    printf("\t-p, --paged\n\t\tThis options turns off direct IO (which is on by default).\n");
    printf("\t-f, --buffered\n\t\tThis options turns off flushing (flushing is on by default).\n" \
           "\t\tThis option is only applicable for write operations.\n");
    printf("\t-m, --do-atime\n\t\tThis options turns on atime updates (off by default).\n");
    printf("\t-a, --append\n\t\tOpen the file in append-only mode (off by default).\n"\
           "\t\tThis option is only applicable for sequential writes.\n");
    printf("\t-u, --dist\n\t\tThe distribution used for random workloads (uniform by default).\n"\
           "\t\tValid options are 'uniform', 'normal', 'pow' (for power law) and const\n"
           "\t\t(for accessing one block). This option is only applicable for random workloads.\n");
    printf("\t-i, --sigma\n\t\tControls the random distribution.\n"\
           "\t\tFor normal distribution, it is the percent of the data one standard deviation away\n"\
           "\t\tfrom the mean (5 by default). If sigma = 5, roughly 68%% of the time only 10%%\n"\
           "\t\tof the data will be accessed. This option is only applicable when distribution is\n"\
           "\t\tset to 'normal'.\n"\
           "\t\tFor power distribution, it is the percentage of the size of the device that will\n"\
           "\t\tbe the mean accessed location (5 by default).\n");
    printf("\t-l, --local-fd\n\t\tUse a thread local file descriptor (by default file descriptors\n"\
           "\t\tare shared across threads).\n");
    printf("\t-n, --silent\n\t\tNon-interactive mode. Won't ask for write confirmation, and will\n"\
           "\t\tprint machine readable output in the following format:\n"\
           "\t\t[ops per second] [MB/sec] [min ops per second] [max ops per second] [standard deviation]\n");
    printf("\t-j, --offset\n\t\tThe offset in the file to start operations from.\n");
    printf("\t\tBy default, this value is set to zero.\n");
    printf("\t\tOffset can also be specified in units other than bytes by appending 'k', 'm', 'g',\n");
    printf("\t\tor '%%'.\n");

    printf("\t-e, --length\n\t\tThe length from the offset in the file to perform operations on.\n");
    printf("\t\tBy default, this value is set to the length of the file or the device.\n");
    printf("\t\tLength can also be specified in units other than bytes by appending 'k', 'm', 'g',\n");
    printf("\t\tor '%%'.\n");

    printf("\t-g, --sample-step\n\t\tThe timestep between IOPS report samples (in milliseconds).\n");
    printf("\t\tDefaults to 1000ms. If set to zero, reports latency of every operation.\n");

    printf("\t-z, --pause\n\t\tThe timestep to wait between a completion of an operation and execution\n");
    printf("\t\tof the next operation in microseconds. Defaults to zero.\n");    

    printf("\t--drop-caches\n\t\tAsks the kernel to drop the cache before running the benchmark.\n");

    printf("\t--output\n\t\tA file name to write detailed data output to at each sample step.\n");
    
    exit(0);
}

off64_t parse_size(char *size, off64_t full_length) {
    if(!size[0])
        return -1;

    int len = strlen(size);
    if(size[len - 1] == '%') {
        return (long long)((float)full_length / 100.0f * atoll(size));
    } else if(size[len - 1] == 'k') {
        return atoll(size) * 1024L;
    } else if(size[len - 1] == 'm') {
        return atoll(size) * 1024L * 1024L;
    } else if(size[len - 1] == 'g') {
        return atoll(size) * 1024L * 1024L * 1024L;
    } else {
        return atoll(size);
    }
}

void parse_duration(char *duration, workload_config_t *config) {
    if(!duration[0])
        return;

    off64_t res;
    int len = strlen(duration);
    if(duration[0] == 'i' && len == 1) {
        config->duration_unit = dut_interactive;
        config->duration = 0;
    } else {
        config->duration_unit = dut_space;
        config->duration = parse_size(duration, config->device_length);
        if(duration[len - 1] != '%' &&
           duration[len - 1] != 'k' &&
           duration[len - 1] != 'm' &&
           duration[len - 1] != 'g')
        {
            config->duration_unit = dut_time;
        }
    }
}

void parse_length(char *length, workload_config_t *config) {
    config->length = parse_size(length, config->device_length);
}

void parse_offset(char *length, workload_config_t *config) {
    config->offset = parse_size(length, config->device_length);
}

void parse_block_size(char *length, workload_config_t *config) {
    config->block_size = parse_size(length, config->device_length);
}

void parse_stride(char *length, workload_config_t *config) {
    config->stride = parse_size(length, config->device_length);
}

void parse_options(int argc, char *argv[], workload_config_t *config) {
    char duration_buf[256];
    duration_buf[0] = 0;
    init_workload_config(config);
    optind = 1; // reinit getopt
    char *length_arg = NULL;
    char *offset_arg = NULL;
    char *block_size_arg = NULL;
    char *stride_arg = NULL;
    while(1)
    {
        struct option long_options[] =
            {
                {"block_size",   required_argument, 0, 'b'},
                {"duration", required_argument, 0, 'd'},
                {"offset", required_argument, 0, 'j'},
                {"length", required_argument, 0, 'e'},
                {"threads", required_argument, 0, 'c'},
                {"stride", required_argument, 0, 's'},
                {"workload", required_argument, 0, 'w'},
                {"type", required_argument, 0, 't'},
                {"queue-depth", required_argument, 0, 'q'},
                {"direction", required_argument, 0, 'r'},
                {"operation", required_argument, 0, 'o'},
                {"dist", required_argument, 0, 'u'},
                {"sigma", required_argument, 0, 'i'},
                {"sample-step", required_argument, 0, 'g'},
                {"pause", required_argument, 0, 'z'},
                {"paged", no_argument, &config->direct_io, 0},
                {"buffered", no_argument, &config->buffered, 1},
                {"do-atime", no_argument, &config->do_atime, 1},
                {"append", no_argument, &config->append_only, 1},
                {"local-fd", no_argument, &config->local_fd, 1},
                {"silent", no_argument, &config->silent, 1},
                {"drop-caches", no_argument, &config->drop_caches, 1},
                {"output", required_argument, 0, OUTPUT_FLAG},
                {"eventfd", no_argument, &config->use_eventfd, 1},		
                {0, 0, 0, 0}
            };

        int option_index = 0;
        int c = getopt_long(argc, argv, "b:d:c:w:t:q:r:s:o:u:i:e:j:g:z:mpfaln", long_options, &option_index);
     
        /* Detect the end of the options. */
        if (c == -1)
            break;
     
        switch (c)
        {
        case 0:
            break;
        case 'b':
            block_size_arg = optarg;
            break;
            
        case 'd':
            strcpy(duration_buf, optarg);
            break;
     
        case 'e':
            length_arg = optarg;
            break;
     
        case 'j':
            offset_arg = optarg;
            break;
     
        case 'c':
            config->threads = atoi(optarg);
            break;
     
        case 's':
            stride_arg = optarg;
            break;
     
        case 'p':
            config->direct_io = 0;
            break;
     
        case 'l':
            config->local_fd = 1;
            break;
     
        case 'n':
            config->silent = 1;
            break;
     
        case 'f':
            config->buffered = 1;
            break;
     
        case 'm':
            config->do_atime = 1;
            break;
     
        case 'a':
            config->append_only = 1;
            break;
     
        case 'w':
            if(strcmp(optarg, "seq") == 0)
                config->workload = wl_seq;
            else if(strcmp(optarg, "rnd") == 0)
                config->workload = wl_rnd;
            else
                check("Invalid workload", 1);
            break;

        case 't':
            if(strcmp(optarg, "stateful") == 0)
                config->io_type = iot_stateful;
            else if(strcmp(optarg, "stateless") == 0)
                config->io_type = iot_stateless;
            else if(strcmp(optarg, "paio") == 0)
                config->io_type = iot_paio;
            else if(strcmp(optarg, "naio") == 0)
                config->io_type = iot_naio;
            else if(strcmp(optarg, "mmap") == 0)
                config->io_type = iot_mmap;
            else
                check("Invalid IO type", 1);
            break;
     
        case 'q':
            config->queue_depth = atoi(optarg);
            break;
            
        case 'r':
            if(strcmp(optarg, "forward") == 0)
                config->direction = opd_forward;
            else if(strcmp(optarg, "backward") == 0)
                config->direction = opd_backward;
            else
                check("Invalid direction", 1);
            break;
     
        case 'o':
            if(strcmp(optarg, "read") == 0)
                config->operation = op_read;
            else if(strcmp(optarg, "write") == 0)
                config->operation = op_write;
            else if(strcmp(optarg, "trim") == 0)
                config->operation = op_trim;
            else
                check("Invalid operation", 1);
            break;

        case 'u':
            if(strcmp(optarg, "uniform") == 0)
                config->dist = rdt_uniform;
            else if(strcmp(optarg, "normal") == 0)
                config->dist = rdt_normal;
            else if(strcmp(optarg, "pow") == 0)
                config->dist = rdt_power;
            else if(strcmp(optarg, "const") == 0)
                config->dist = rdt_const;
            else
                check("Invalid distribution", 1);
            break;

        case 'i':
            config->sigma = atoi(optarg);
            break;

        case 'g':
            config->sample_step = atoi(optarg);
            break;

        case 'z':
            config->pause_interval = atoi(optarg);
            break;

        case OUTPUT_FLAG:
            strncpy(config->output_file, optarg, DEVICE_NAME_LENGTH);
            break;

        case '?':
            /* getopt_long already printed an error message. */
            usage(argv[0]);
            break;
     
        default:
            usage(argv[0]);
        }
    }

    if(config->pause_interval != 0
       && (config->io_type == iot_paio || config->io_type == iot_naio)) {
        check("Pauses aren't implemented for paio and naio backends", 1);
    }

    if(config->workload == wl_rnd && config->direction == opd_backward)
        check("Direction can only be used for a sequential workload", 1);

    if(config->append_only && (config->workload == wl_rnd || config->operation == op_read))
        check("Append-only cannot be run with random or read workloads", 1);

    if(config->buffered == 1 && config->direct_io == 1)
        check("Can't turn on buffering with direct IO", 1);

    if (optind < argc) {
        strncpy(config->device, argv[optind++], DEVICE_NAME_LENGTH);
        config->device[DEVICE_NAME_LENGTH - 1] = 0;
    }
    else
        usage(argv[0]);

    if(config->sigma == -1) {
        if(config->dist == rdt_normal)
            config->sigma = 5;
        else if(config->dist == rdt_power)
            config->sigma = 5;
    }
    else if(config->dist != rdt_normal && config->dist != rdt_power)
        check("Sigma is only valid for normal and power distributions", 1);

    if(config->threads < 1)
        check("Please use at least one thread", 1);

    if(config->direct_io && config->io_type == iot_mmap)
        check("Can't use mmap with direct IO (use --paged)", 1);

    if(config->threads > 1 && config->io_type == iot_stateful && !config->local_fd)
        check("Can't use shared file descriptor with stateful IO on multiple threads"\
              " (use -t stateless or --local-fd)", 1);

    if(config->workload == wl_seq && config->operation == op_write && config->direction == opd_forward &&
        config->io_type == iot_mmap)
        check("Memory mapping isn't implemented where remapping might be required", 1);

    if(config->operation == op_write && !config->silent) {
        while(true) {
            printf("Are you sure you want to write to %s [y/N]? ", config->device);
            int response = getc(stdin);
            if(response == EOF) {
                printf("\n");
                exit(-1);
            } else if(response == '\n') {
                printf("\n");
                exit(-1);
            } else if(response == 'n' || response == 'N') {
                exit(-1);
            } else if(response == 'y' || response == 'Y') {
                break;
            } else {
                getc(stdin);
            }
        }
    }

    check("Queue depth is only relevant for paio, and naio workloads",
          config->queue_depth > 1 && (config->io_type != iot_paio && config->io_type != iot_naio));

    check("Eventfd is only relevant for naio workloads",
          config->use_eventfd == 1 && config->io_type != iot_naio);

    config->device_length = get_device_length(config->device);

    if(length_arg) {
        parse_length(length_arg, config);
    }
    if(offset_arg) {
        parse_offset(offset_arg, config);
    }
    if(block_size_arg) {
        parse_block_size(block_size_arg, config);
    }
    if(stride_arg) {
        parse_stride(stride_arg, config);
    }
    
    // Set the length
    if(config->length == 0) {
        config->length = config->device_length;
    } else {
        config->length = std::min(config->length, config->device_length);
    }
    if(config->offset + config->length > config->device_length) {
        config->length = config->device_length - config->offset;
    }

    parse_duration(duration_buf, config);
    if(config->duration_unit == dut_interactive && config->threads > 1) {
        check("Cannot run in interactive mode with multiple threads", 1);
    }
}

void print_size(off64_t size) {
    long long hl = (long long)((float)size / 1024.0f / 1024.0f / 1024.0f);
    if(hl != 0)
        printf("%lldGB", hl);
    else {
        long long hl = (long long)((float)size / 1024.0f / 1024.0f);
        if(hl != 0)
            printf("%lldMB", hl);
        else {
            long long hl = (long long)((float)size / 1024.0f);
            if(hl != 0 )
                printf("%lldKB", hl);
            else
                printf("%lldb", (long long)size);
        }
    }
}

void print_status(off64_t length, workload_config_t *config) {
    if(config->silent)
        return;
    
    printf("Benchmarking results for [%s] (", config->device);
    print_size(length);
    printf(")\n");
        
    printf("[duration: ");
    if(config->duration_unit == dut_time) {
        printf("%llds, ", config->duration);
    } else if(config->duration_unit == dut_space) {
        print_size(config->duration);
        printf(", ");
    } else {
        printf("interactive, ");
    }

    if(config->offset != 0) {
        printf("offset: ");
        print_size(config->offset);
        printf(", ");
    }
    if(config->length != config->device_length) {
        printf("length: ");
        print_size(config->length);
        printf(", ");
    }

    printf("threads: %d, ", config->threads);

    if(config->threads > 1) {
        printf("fd: ");
        if(config->local_fd)
            printf("local, ");
        else
            printf("shared, ");
    }
    
    printf("operation: ");
    if(config->operation == op_write)
        printf("write, ");
    else if(config->operation == op_read)
        printf("read, ");
    else if(config->operation == op_trim)
        printf("trim, ");
    else
        check("Unknown operation", 1);
    
    if(config->operation == op_write) {
        printf("buffering: ");
        if(config->buffered)
            printf("on, ");
        else
            printf("off, ");
    }
    printf("block size: %db, ", config->block_size);
    if(config->workload == wl_seq)
        printf("stride: %db, ", config->stride);
    else
        printf("alignment: %db, ", config->stride);

    printf("workload: ");
    if(config->workload == wl_rnd)
        printf("rnd, ");
    else if(config->workload == wl_seq)
        printf("seq, ");
    else
        check("Invalid workload", 1);

    if(config->workload == wl_rnd) {
        printf("dist: ");
        if(config->dist == rdt_const)
            printf("const, ");
        else if(config->dist == rdt_uniform)
            printf("uniform, ");
        else if(config->dist == rdt_normal)
            printf("normal, ");
        else if(config->dist == rdt_power)
            printf("zipfian, ");
        else
            check("Invalid distribution", 1);
        if(config->dist == rdt_normal || config->dist == rdt_power)
            printf("sigma: %d, ", config->sigma);
    }
    
    printf("direct IO: ");
    if(config->direct_io)
        printf("on, ");
    else
        printf("off, ");
    
    printf("IO type: ");
    if(config->io_type == iot_stateful)
        printf("stateful, ");
    else if(config->io_type == iot_stateless)
        printf("stateless, ");
    else if(config->io_type == iot_paio)
        printf("posix AIO, ");
    else if(config->io_type == iot_naio)
        printf("native AIO, ");
    else if(config->io_type == iot_mmap)
        printf("mmap, ");
    else
        check("Invalid IO type", 1);

    if(config->io_type == iot_paio || config->io_type == iot_naio) {
        printf("queue depth: %d, ", config->queue_depth);
    }
    
    if(config->io_type == iot_naio) {
        printf("eventfd: ");
        if(config->use_eventfd)
            printf("yes, ");
        else
            printf("no, ");
    }
    
    if(config->workload == wl_seq) {
        printf("direction: ");
        if(config->direction == opd_forward)
            printf("forward, ");
        else if(config->direction == opd_backward)
            printf("backward, ");
        else
            check("Invalid direction", 1);
            
    }

    if(config->workload == wl_seq && config->operation == op_write) {
        printf("append-only: ");
        if(config->append_only)
            printf("on, ");
        else
            printf("off, ");
    }
    
    printf("time updates: ");
    if(config->do_atime)
        printf("on");
    else
        printf("off");

    printf(", sample step: %d", config->sample_step);
    if(config->sample_step != 0)
        printf("ms");
    
    printf(", pause interval: %ld", config->pause_interval);
    if(config->pause_interval != 0)
        printf("us");
    
    printf("]\n");
}

