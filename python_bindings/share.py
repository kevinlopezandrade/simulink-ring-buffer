import sys
import numpy as np
from multiprocessing import shared_memory
from ctypes import Structure, c_size_t, c_uint, c_bool, c_uint8, c_ulong, c_long, cdll, cast, POINTER, pointer, sizeof, c_char_p
import cbor2

BUFFER_SIZE = 256
CBOR_BUFFER_SIZE = 200


class timespec(Structure):
    _fields_ = [
            ("tv_sec", c_long),
            ("tv_nsec", c_long)
    ]

class ReadToken(Structure):
    _fields_ = [
            ("idx", c_uint),
            ("initialized", c_bool)
    ]

class Message(Structure):
    _fields_ = [
            ("data", c_uint8 * CBOR_BUFFER_SIZE),
            ("wait", c_bool),
            ("checksum", c_ulong),
            ("timestamp", timespec)
    ]

class RingBuffer(Structure):
    _fields_ = [
            ("write_idx", c_uint),
            ("oldest_idx",c_uint),
            ("filled", c_bool),
            ("buf_size", c_uint),
            ("cbor_buffer_size", c_uint),
            ("buffer", Message * BUFFER_SIZE)
    ]


libringbuffer = cdll.LoadLibrary("./libringbuffer.so")
libringbuffer.publish.argtypes = [POINTER(RingBuffer), POINTER(c_uint8), c_size_t]

shm = shared_memory.SharedMemory(name="OnlyRead", size=sizeof(RingBuffer), create=True, track=False)
ringbuffer = RingBuffer.from_buffer(shm.buf)

ringbuffer.write_idx = 0
ringbuffer.oldest_idx = 0
ringbuffer.filled = False
ringbuffer.buf_size = BUFFER_SIZE
ringbuffer.cbor_buffer_size = CBOR_BUFFER_SIZE

try:
    while True:
        cborbuffer = cbor2.dumps({"m": [[np.random.rand()]], "y": [[7.0]]})
        cborbuffer_ptr = cast(c_char_p(cborbuffer), POINTER(c_uint8))
        cborbuffer_len = len(cborbuffer)

        # The issue with the checksum is the following one:
        # I need to specify the max buffer size and not the buffer len size.
        libringbuffer.publish(pointer(ringbuffer), cborbuffer_ptr, CBOR_BUFFER_SIZE)

except KeyboardInterrupt:
    del ringbuffer
    shm.unlink()
