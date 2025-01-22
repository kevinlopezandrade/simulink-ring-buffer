#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>      // For O_RDONLY
#include <sys/mman.h>   // For mmap
#include <sys/stat.h>   // For shm_open, fstat
#include <unistd.h>     // For close
#include <string.h>     // For strerror
#include <errno.h>      // For errno
#include <stdbool.h>

#include "benchmark.h"
#include "ncctools/checksum.h"
#include "ncctools/ringbuffer.h"

static long elapsed(struct timespec* start, struct timespec* end){
    long seconds_diff = end->tv_sec - start->tv_sec;
    long nanoseconds_diff = end->tv_nsec - start->tv_nsec;
    long elapsed = seconds_diff * 1e9L + nanoseconds_diff;

    return elapsed;
}

int
main(int argc, char *argv[])
{
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <shm_name>\n", argv[0]);
        return EXIT_FAILURE;
    }

    const char *shm_name = argv[1];
    int shm_fd;
    void *shm_ptr;
    struct stat shm_stat;

    /* Open the shared memory object for reading */
    shm_fd = shm_open(shm_name, O_RDONLY, S_IWUSR);
    if (shm_fd == -1) {
        fprintf(stderr, "Error opening shared memory object %s: %s\n", shm_name, strerror(errno));
        return EXIT_FAILURE;
    }

    /* Get the size of the shared memory object */
    if (fstat(shm_fd, &shm_stat) == -1) {
        fprintf(stderr, "Error getting the size of shared memory object %s: %s\n", shm_name, strerror(errno));
        close(shm_fd);
        return EXIT_FAILURE;
    }

    /* Memory map the shared memory object for reading only */
    shm_ptr = mmap(NULL, sizeof(RingBuffer), PROT_READ, MAP_SHARED, shm_fd, 0);
    if (shm_ptr == MAP_FAILED) {
        fprintf(stderr, "Error mapping shared memory object %s: %s\n", shm_name, strerror(errno));
        close(shm_fd);
        return EXIT_FAILURE;
    }

    /* Now shm_ptr points to the shared memory and can be read. */
    printf("Successfully mapped shared memory object %s (size: %ld bytes).\n", shm_name, shm_stat.st_size);

    RingBuffer* ring_buffer = (RingBuffer*)shm_ptr;
    ReadToken read_token = {.idx = 0, .initialized = false};
    
    long counter = 0;
    Benchmark benchmark;

    setup_benchmark(&benchmark);
    while (counter < 1e5) {
        Message msg = read_next(&read_token, ring_buffer);
        if (!msg.wait) {
            counter += 1;
            if ( crc((unsigned char *)&msg.data, CBOR_BUFFER_SIZE) != msg.checksum ) {
                fprintf(stderr, "Checksum failed");
            }
            benchmark_step(&benchmark, &msg);
        }
    };
    evaluate(&benchmark, counter, CBOR_BUFFER_SIZE);


    // Cleanup: Unmap and close the shared memory object
    if (munmap(shm_ptr, shm_stat.st_size) == -1) {
        fprintf(stderr, "Error unmapping shared memory: %s\n", strerror(errno));
    }

    close(shm_fd);
    return EXIT_SUCCESS;
}
