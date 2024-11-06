main.m: mmapwriter.mexa64 read.out mmapreader.mexa64

mmapwriter.mexa64: mmapwriter.c libringbuffer.so checksum.o
	LANG=C mex mmapwriter.c libringbuffer.so checksum.o libtinycbor.a

mmapreader.mexa64: mmapreader.c libringbuffer.so checksum.o
	LANG=C mex mmapreader.c libringbuffer.so checksum.o libtinycbor.a

libringbuffer.so: ringbuffer.c ringbuffer.h checksum.o
	gcc -O3 -fPIC -shared -o libringbuffer.so ringbuffer.c checksum.o libtinycbor.a

checksum.o: checksum.c checksum.h
	gcc -O3 -fPIC -c checksum.c

benchmark.o: benchmark.c benchmark.h
	gcc -O3 -c benchmark.c

read.out: read.c libringbuffer.so benchmark.o
	gcc read.c -O3 -o read.out libringbuffer.so benchmark.o checksum.o -lm libtinycbor.a
