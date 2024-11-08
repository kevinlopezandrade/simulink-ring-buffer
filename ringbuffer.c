#include <stdbool.h>
#include <stdatomic.h>
#include <time.h>
#include <string.h>

#include "ringbuffer.h"
#include "checksum.h"

static Message NULL_MSG = {.data = 0, .wait = true};

unsigned int
wrap(unsigned int idx)
{
    return idx & (BUFFER_SIZE - 1);
}

void
publish(RingBuffer* ring_buffer, float value)
{
    unsigned int wrapped_idx;
    wrapped_idx = wrap(ring_buffer->write_idx);

    /* Pointer to the position to write. */
    Message* msg = &ring_buffer->buffer[wrapped_idx];

    /* Write the value. */
    msg->data = value;
    // memset(msg->data, 0, 1024 * 1024);
    msg->wait = false;
    
    // msg->checksum = crc((unsigned char*) msg->data, sizeof(uint8_t[1024*1024]));
    msg->checksum = crc((unsigned char*) &value, sizeof(float));


    /* Add timestamp. */
    clock_gettime(CLOCK_MONOTONIC, &msg->timestamp);

    /* Increase the index of the infinite memory. Needs to bea atomic since a
    subscriber might read from this index. */
    __sync_fetch_and_add(&ring_buffer->write_idx, 1);


    /* If full then advance the oldest_idx by fixed amount. */
    if (ring_buffer->filled) {
        ring_buffer->oldest_idx = ring_buffer->write_idx - (ring_buffer->buf_size - 1);
    }
    else if (wrap(ring_buffer->write_idx) == 0) {
        ring_buffer->filled = true;
    }

}

Message
read_next(ReadToken* token, RingBuffer* ring_buffer)
{
    unsigned int oldest_idx;
    unsigned int desired;
    unsigned int next_write;
    unsigned int wgap;

    oldest_idx = ring_buffer->oldest_idx;

    if (!token->initialized) {
        desired = oldest_idx;
    } else {
        /* Also referring to infinite memory */
        desired = token->idx + 1;
    }

    /* If write_idx changes in the producer process, the gap is still valid. */
    next_write = ring_buffer->write_idx;
    if (desired == next_write) {
        return NULL_MSG;
    }

    wgap = next_write - desired;
    if (wgap < ring_buffer->buf_size) {
        token->idx = desired;
    } else {
        /* I need to double confirmt the safety of this.
        I might be pointing here to an oudated value.
        Since oldest_idx might have been updated already by the
        publisher thread. */
        token->idx = oldest_idx;
    }

    token->initialized = true;

    /* Before here things might have already changed in the indices. Thing
    about a gap that is big enough and oldest_idx is where I can write now,
    if its has been increased then I'll be pointing to a new data instead to
    the oldest one in a ring buffer. */
    return ring_buffer->buffer[wrap(token->idx)];
}
