#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <time.h>
#include <assert.h>
#include <math.h>
#include <stdint.h>

#include "benchmark.h"
#include "ncctools/ringbuffer.h"

/* Code adapted from  https://github.com/goldsborough/ipc-bench/ all
credits to authoers of that repo. */

bench_t
now()
{
    /* Maybe I need to change this to pass a pointer
    ts already to avoid allocating in the stack
    every time I need to grab the timer. */
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);

    return ts.tv_sec * 1e9 + ts.tv_nsec;
}

void
setup_benchmark(Benchmark* bench) {
	bench->maximum = 0;
	bench->minimum = INT32_MAX;
	bench->sum = 0;
	bench->squared_sum = 0;
	bench->total_start = now();
}

void
benchmark_step(Benchmark* bench, NCCToolsMessage* msg)
{
    const bench_t time = now() - (msg->timestamp.tv_sec * 1e9 +
        msg->timestamp.tv_nsec);

	if (time < bench->minimum) {
		bench->minimum = time;
	}

	if (time > bench->maximum) {
		bench->maximum = time;
	}

	bench->sum += time;
	bench->squared_sum += (time * time);
}


void
evaluate(Benchmark* bench, int count, int size)
{
	assert(count > 0);
	const bench_t total_time = now() - bench->total_start;
	const double average = ((double)bench->sum) / count;

	double sigma = bench->squared_sum / count;
	sigma = sqrt(sigma - (average * average));

	int messageRate = (int)(count / (total_time / 1e9));

	printf("\n============ RESULTS ================\n");
	printf("Message size:       %d\n", size);
	printf("Message count:      %d\n", count);
	printf("Total duration:     %.3f\tms\n", total_time / 1e6);
	printf("Average duration:   %.3f\tus\n", average / 1000.0);
	printf("Minimum duration:   %.3f\tus\n", bench->minimum / 1000.0);
	printf("Maximum duration:   %.3f\tus\n", bench->maximum / 1000.0);
	printf("Standard deviation: %.3f\tus\n", sigma / 1000.0);
	printf("Message rate:       %d\tmsg/s\n", messageRate);
	printf("=====================================\n");
}
