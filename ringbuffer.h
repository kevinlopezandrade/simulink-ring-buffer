#ifndef BUFFER_RING_H
#define BUFFER_RING_H

#include <stdint.h>

#define BUFFER_SIZE 256
#define CBOR_BUFFER_SIZE 100

typedef struct
{
    unsigned int idx;
    bool initialized;
} ReadToken;

typedef struct
{
    uint8_t data[CBOR_BUFFER_SIZE];
    bool wait;
    unsigned long checksum;
    struct timespec timestamp;
} Message;

// TODO: Add a length to the struct.

typedef struct
{
    /* Assuming infinite memory */
    _Atomic(unsigned int) write_idx;
    _Atomic(unsigned int) oldest_idx;
    _Atomic(bool) filled;
    unsigned int buf_size;
    unsigned int cbor_buffer_size;
    Message buffer[BUFFER_SIZE];
} RingBuffer;

unsigned int
wrap(unsigned int idx);

void
publish(RingBuffer* ring_buffer, uint8_t* cborBuffer, size_t bufferLen);

Message
read_next(ReadToken* token, RingBuffer* ring_buffer);

#endif
