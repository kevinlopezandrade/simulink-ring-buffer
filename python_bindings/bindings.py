import sys
from multiprocessing import shared_memory
from ctypes import Structure, c_uint, c_bool, c_uint8, c_ulong, c_long, cdll, cast, POINTER, pointer
import cbor2

BUFFER_SIZE = 256
CBOR_BUFFER_SIZE = 40


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
            ("buffer", Message * BUFFER_SIZE)
    ]


libringbuffer = cdll.LoadLibrary("./libringbuffer.so")
libringbuffer.read_next.argtypes = [POINTER(ReadToken), POINTER(RingBuffer)]
libringbuffer.read_next.restype = Message

shm = shared_memory.SharedMemory(name=sys.argv[1], create=False, track=False)
ringbuffer = RingBuffer.from_buffer(shm.buf)
read_token = ReadToken(0, False)

while True:
    msg: Message = libringbuffer.read_next(pointer(read_token), pointer(ringbuffer))
    signals = cbor2.loads(msg.data)
