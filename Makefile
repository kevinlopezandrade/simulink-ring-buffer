main.m: timestwo.mexa64 read.out reader.mexa64

timestwo.mexa64: timestwo.c ringbuffer.o checksum.o
	LANG=C mex timestwo.c ringbuffer.o checksum.o

reader.mexa64: reader.c ringbuffer.o checksum.o
	LANG=C mex reader.c ringbuffer.o checksum.o

ringbuffer.o: ringbuffer.c ringbuffer.h
	gcc -O3 -c ringbuffer.c

checksum.o: checksum.c checksum.h
	gcc -O3 -c checksum.c

benchmark.o: benchmark.c benchmark.h
	gcc -O3 -c benchmark.c

read.out: read.c ringbuffer.o benchmark.o
	gcc read.c -O3 -o read.out ringbuffer.o benchmark.o checksum.o -lm
