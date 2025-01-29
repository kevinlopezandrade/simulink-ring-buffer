#ifndef BUFFER_RING_H
#define BUFFER_RING_H

#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <time.h>

#define BUFFER_SIZE 256
#define CBOR_BUFFER_SIZE 200

typedef struct {
  unsigned int idx;
  bool initialized;
} NCCToolsReadToken;

typedef struct {
  uint8_t data[CBOR_BUFFER_SIZE];
  bool wait;
  unsigned long checksum;
  struct timespec timestamp;
} NCCToolsMessage;

// TODO: Add a length to the struct.

typedef struct {
  /* Assuming infinite memory */
  _Atomic(unsigned int) write_idx;
  _Atomic(unsigned int) oldest_idx;
  _Atomic(bool) filled;
  unsigned int buf_size;
  unsigned int cbor_buffer_size;
  NCCToolsMessage buffer[BUFFER_SIZE];
} NCCToolsRingBuffer;

void ncctools_publish(NCCToolsRingBuffer *ring_buffer, uint8_t *cborBuffer,
                      size_t bufferLen);

NCCToolsMessage ncctools_read_next(NCCToolsReadToken *token,
                                   NCCToolsRingBuffer *ring_buffer);

#endif
