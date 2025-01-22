cc_library(
    name = "benchmark",
    srcs = ["benchmark.c"],
    hdrs = ["benchmark.h"],
    linkstatic = True,
    deps = [
        "//ncctools:ringbuffer"
    ]
)

cc_binary(
    name = "read_benchmark",
    srcs = ["read_benchmark.c"],
    deps = [
        ":benchmark",
        "//ncctools:ringbuffer",
        "//ncctools:checksum"
    ],
)
