import sys
from ctypes import POINTER, c_char_p, c_uint8, cast, pointer, sizeof
from multiprocessing import shared_memory

import cbor2
import numpy as np

from pyncctools import BUFFER_SIZE, CBOR_BUFFER_SIZE, RingBuffer, publish

shm = shared_memory.SharedMemory(
    name="OnlyRead", size=sizeof(RingBuffer), create=True, track=False
)
ringbuffer = RingBuffer.from_buffer(shm.buf)

ringbuffer.write_idx = 0
ringbuffer.oldest_idx = 0
ringbuffer.filled = False
ringbuffer.buf_size = BUFFER_SIZE
ringbuffer.cbor_buffer_size = CBOR_BUFFER_SIZE

try:
    while True:
        cborbuffer = cbor2.dumps({
            "x": [[3.0]],
            "inner": {
                "y": [[np.random.rand()]],
                "inner2": {
                    "y": [[1.0]]
                    }
                }
            }
        )
        cborbuffer_ptr = cast(c_char_p(cborbuffer), POINTER(c_uint8))
        publish(pointer(ringbuffer), cborbuffer_ptr, CBOR_BUFFER_SIZE)

except KeyboardInterrupt:
    del ringbuffer
    shm.unlink()
