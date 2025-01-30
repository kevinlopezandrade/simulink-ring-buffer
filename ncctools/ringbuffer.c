#include <stdatomic.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>
#include <tinycbor/cbor.h>

#include "checksum.h"
#include "ringbuffer.h"

static NCCToolsMessage NULL_MSG = {.wait = true};

static unsigned int wrap(unsigned int idx) { return idx & (BUFFER_SIZE - 1); }

void ncctools_publish(NCCToolsRingBuffer *ring_buffer, uint8_t *cbor_buffer,
                      size_t buffer_len) {
  unsigned int wrapped_idx;

  wrapped_idx = wrap(ring_buffer->write_idx);

  /* Pointer to the position to write. */
  NCCToolsMessage *msg = &ring_buffer->buffer[wrapped_idx];

  /* Copy the cbor buffer to the msg data. */
  memcpy(msg->data, cbor_buffer, buffer_len);

  msg->wait = false;
  msg->checksum = ncctools_crc((unsigned char *)msg->data, buffer_len);

  /* Add timestamp. */
  clock_gettime(CLOCK_MONOTONIC, &msg->timestamp);

  /* Increase the index of the infinite memory. Needs to bea atomic since a
  subscriber might read from this index. */
  __sync_fetch_and_add(&ring_buffer->write_idx, 1);

  /* If full then advance the oldest_idx by fixed amount. */
  if (ring_buffer->filled) {
    ring_buffer->oldest_idx =
        ring_buffer->write_idx - (ring_buffer->buf_size - 1);
  } else if (wrap(ring_buffer->write_idx) == 0) {
    ring_buffer->filled = true;
  }
}

NCCToolsMessage ncctools_read_next(NCCToolsReadToken *token,
                                   NCCToolsRingBuffer *ring_buffer) {
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
    /* I need to double confirm the safety of this.
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
