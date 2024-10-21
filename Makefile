main.m: timestwo.mexa64 read.out

timestwo.mexa64: timestwo.c ringbuffer.o
	LANG=C mex timestwo.c ringbuffer.o

ringbuffer.o: ringbuffer.c ringbuffer.h
	gcc -O3 -c ringbuffer.c

benchmark.o: benchmark.c benchmark.h
	gcc -O3 -c benchmark.c

read.out: read.c ringbuffer.o benchmark.o
	gcc read.c -O3 -o read.out ringbuffer.o  benchmark.o -lm
