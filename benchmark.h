#ifndef BENCHMARK_H
#define BENCHMARK_H

#include "ringbuffer.h"

typedef unsigned long long bench_t;

typedef struct
{
	// Start of the total benchmarking
	bench_t total_start;

	// Minimum time
	bench_t minimum;

	// Maximum time
	bench_t maximum;

	// Sum (for averaging)
	bench_t sum;

	// Squared sum (for standard deviation)
	bench_t squared_sum;

} Benchmark;


bench_t
now();

void
setup_benchmark(Benchmark* bench);

void
benchmark_step(Benchmark* bench, Message* msg);

void
evaluate(Benchmark* bench, int size, int count);

#endif
