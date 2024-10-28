main.m: mmapwriter.mexa64 read.out reader.mexa64

mmapwriter.mexa64: mmapwriter.c ringbuffer.o checksum.o
	LANG=C mex mmapwriter.c ringbuffer.o checksum.o libtinycbor.a

reader.mexa64: reader.c ringbuffer.o checksum.o
	LANG=C mex reader.c ringbuffer.o checksum.o libtinycbor.a

ringbuffer.o: ringbuffer.c ringbuffer.h
	gcc -O3 -c ringbuffer.c

checksum.o: checksum.c checksum.h
	gcc -O3 -c checksum.c

benchmark.o: benchmark.c benchmark.h
	gcc -O3 -c benchmark.c

read.out: read.c ringbuffer.o benchmark.o
	gcc read.c -O3 -o read.out ringbuffer.o benchmark.o checksum.o -lm libtinycbor.a
