# Expose the tinycbor libraries for the project.

prebuilt_cxx_library(
    name = "tinycbor",
    soname = "libtinycbor.so",
    static_lib = "tinycbor/lib/libtinycbor.a",
    shared_lib = "tinycbor/lib/libtinycbor.so",
    exported_headers = glob(["tinycbor/src/*.h"]),
    visibility = ["PUBLIC"]
)
