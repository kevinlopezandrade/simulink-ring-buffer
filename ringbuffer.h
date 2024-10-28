#ifndef BUFFER_RING_H
#define BUFFER_RING_H

#include <stdint.h>

#define BUFFER_SIZE 256

typedef struct
{
    unsigned int idx;
    bool initialized;
} ReadToken;

typedef struct
{
    // uint8_t data[1024*1024];
    uint8_t data[16];
    bool wait;
    unsigned long checksum;
    struct timespec timestamp;
} Message;

typedef struct
{
    /* Assuming infinite memory */
    _Atomic(unsigned int) write_idx;
    _Atomic(unsigned int) oldest_idx;
    _Atomic(bool) filled;
    unsigned int buf_size;
    Message buffer[BUFFER_SIZE];
} RingBuffer;

unsigned int
wrap(unsigned int idx);

void
publish(RingBuffer* ring_buffer, float value);

Message
read_next(ReadToken* token, RingBuffer* ring_buffer);

#endif
