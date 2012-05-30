# Usage: 
	./rebench [OPTIONS] DEVICE

	Alternatively, if no arguments are passed on the command line, ./rebench will read
	standard input and read its arguments from there. Multiple lines may be used
	to specify multiple concurrent workloads.

# Arguments:
	DEVICE - device or file name to perform operations on.

# Options:
	-d, --duration
                Duration of the benchmark in seconds.
                Duration can be specified in KB, MB, and GB (add 'k', 'm', or 'g'
                to the end of the number. The benchmark will stop when that much
                data has been processed. If the number is followed by '%',
                the size will be that percentage of the device. If duration is
                set to 'i', rebench will run commands interactively (useful for
                debugging with btrace, and only available in single-threaded mode).
	-c, --threads
                Numbers of threads used to run the benchmark.
	-b, --block_size
                Size of blocks in bytes to use for IO operations.
                Block size can also be specified in units other than bytes by appending
                'k', 'm', 'g', or '%'.
	-s, --stride
                Size of stride in bytes (only applies to sequential workloads).
                Stride can also be specified in units other than bytes by appending
                'k', 'm', 'g', or '%'.
	-w, --workload
                Description of a workload to perform.
                Valid options are 'rnd' for a random load, and 'seq' for a sequential load.
	-t, --type
                Type of IO calls to use.
                Valid options are 'stateful' for read/write IO,
                'stateless' for pread/pwrite type of IO, 'mmap' for
                memory mapping, 'paio' for POSIX asynchronous IO,
                and 'naio' for native OS asynchronous IO.
	-q, --queue-depth
                The number of simultaneous AIO calls.
                Valid only during 'paio', and 'naio' type of runs.
	--eventfd
                Use eventfd for aio completion notification.
                Valid only during 'naio' type of runs. Useful for measuring eventfd overhead.
	-r, --direction
                Direction in which the operations are performed.
                Valid options are 'formward' and 'backward'.
                This option is only applicable to sequential workloads.
	-o, --operation
                The operation to be performed.
                Valid options are 'read', 'write', and 'trim'.
                Note that trim is only available on SSD devices
                via a stateful interface.
	-p, --paged
                This options turns off direct IO (which is on by default).
	-f, --buffered
                This options turns off flushing (flushing is on by default).
                This option is only applicable for write operations.
	-m, --do-atime
                This options turns on atime updates (off by default).
	-a, --append
                Open the file in append-only mode (off by default).
                This option is only applicable for sequential writes.
	-u, --dist
                The distribution used for random workloads (uniform by default).
                Valid options are 'uniform', 'normal', 'pow' (for power law) and const
                (for accessing one block). This option is only applicable for random workloads.
	-i, --sigma
                Controls the random distribution.
                For normal distribution, it is the percent of the data one standard deviation away
                from the mean (5 by default). If sigma = 5, roughly 68% of the time only 10%
                of the data will be accessed. This option is only applicable when distribution is
                set to 'normal'.
                For power distribution, it is the percentage of the size of the device that will
                be the mean accessed location (5 by default).
	-l, --local-fd
                Use a thread local file descriptor (by default file descriptors
                are shared across threads).
	-n, --silent
                Non-interactive mode. Won't ask for write confirmation, and will
                print machine readable output in the following format:
                [ops per second] [MB/sec] [min ops per second] [max ops per second] [standard deviation]
	-j, --offset
                The offset in the file to start operations from.
                By default, this value is set to zero.
                Offset can also be specified in units other than bytes by appending 'k', 'm', 'g',
                or '%'.
	-e, --length
                The length from the offset in the file to perform operations on.
                By default, this value is set to the length of the file or the device.
                Length can also be specified in units other than bytes by appending 'k', 'm', 'g',
                or '%'.
	-g, --sample-step
                The timestep between IOPS report samples (in milliseconds).
                Defaults to 1000ms. If set to zero, reports latency of every operation.
	-z, --pause
                The timestep to wait between a completion of an operation and execution
                of the next operation in microseconds. Defaults to zero.
	--drop-caches
                Asks the kernel to drop the cache before running the benchmark.
	--output
                A file name to write detailed data output to at each sample step.

# R Script
describe.R visualizes latency and throughput statistics. 

	Rscript describe.R <trace1 filename> <trace1 name> <trace2 filename> <trace2 name> ...

# Plot Examples

![Throughput Time Plot][https://github.com/ikatsov/RebenchR/blob/master/examples/throughput_time_plot.png]
