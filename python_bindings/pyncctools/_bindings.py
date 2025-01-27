from ctypes import (
    POINTER,
    Structure,
    c_bool,
    c_char_p,
    c_long,
    c_size_t,
    c_uint,
    c_uint8,
    c_ulong,
    cdll,
)

BUFFER_SIZE = 256
CBOR_BUFFER_SIZE = 200


class timespec(Structure):
    _fields_ = [("tv_sec", c_long), ("tv_nsec", c_long)]


class ReadToken(Structure):
    _fields_ = [("idx", c_uint), ("initialized", c_bool)]


class Message(Structure):
    _fields_ = [
        ("data", c_uint8 * CBOR_BUFFER_SIZE),
        ("wait", c_bool),
        ("checksum", c_ulong),
        ("timestamp", timespec),
    ]


class RingBuffer(Structure):
    _fields_ = [
        ("write_idx", c_uint),
        ("oldest_idx", c_uint),
        ("filled", c_bool),
        ("buf_size", c_uint),
        ("cbor_buffer_size", c_uint),
        ("buffer", Message * BUFFER_SIZE),
    ]


_libringbuffer = cdll.LoadLibrary("libncctools-ringbuffer.so")
_libringbuffer.ncctools_publish.argtypes = [
    POINTER(RingBuffer),
    POINTER(c_uint8),
    c_size_t,
]

_libringbuffer.ncctools_read_next.argtypes = [POINTER(ReadToken), POINTER(RingBuffer)]
_libringbuffer.ncctools_read_next.restype = Message


def publish(
    ringbuffer: POINTER(RingBuffer),
    cborbuffer_ptr: POINTER(c_uint8),
    cborbuffer_size: c_size_t,
):
    _libringbuffer.publish(ringbuffer, cborbuffer_ptr, cborbuffer_size)


def read_next(
    read_token: POINTER(ReadToken), ringbuffer: POINTER(RingBuffer)
) -> Message:
    return _libringbuffer.read_next(read_token, ringbuffer)
