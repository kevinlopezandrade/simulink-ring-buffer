load("@rules_pkg//pkg:mappings.bzl", "pkg_filegroup", "pkg_files", "strip_prefix")
load("@rules_pkg//pkg:tar.bzl", "pkg_tar")
load("@rules_pkg//pkg:deb.bzl", "pkg_deb")

cc_library(
    name = "checksum",
    srcs = ["checksum.c"],
    hdrs = ["checksum.h"],
    visibility = ["//visibility:public"],
)

cc_shared_library(
    name = "checksum_shared",
    shared_lib_name = "libncctools-checksum.so",
    deps = [
        ":checksum"
    ]
)

cc_library(
    name = "ringbuffer",
    srcs = ["ringbuffer.c"],
    hdrs = ["ringbuffer.h"], 
    deps = [
        ":checksum"
    ],
    visibility = ["//visibility:public"],
)

cc_shared_library(
    name = "ringbuffer_shared",
    shared_lib_name = "libncctools-ringbuffer.so",
    deps = [":ringbuffer"],
    dynamic_deps = [
        ":checksum_shared"
    ]
)


# Package Defintions
pkg_files(
    name = "ringbuffer_include",
    srcs = glob(["*.h"]),
    prefix = "include/ncctools"
)

pkg_files(
    name = "ringbuffer_lib",
    srcs = [
        ":checksum_shared",
        ":ringbuffer_shared"
    ],
    prefix = "lib/ncctools"
)

pkg_filegroup(
    name = "ncctools_tree",
    srcs = [
        ":ringbuffer_lib",
        ":ringbuffer_include"
    ],
    prefix = "/usr"
)

pkg_tar(
    name = "ncctools-tar",
    srcs = [
        ":ncctools_tree"
    ]
)

pkg_deb(
    name = "ncctools",
    package = "ncctools",
    data = ":ncctools-tar",
    maintainer = "Kevin Lopez",
    version = "0.1",
    description = "Shared libraries for the Neural Computation and Cognition",
    architecture = "x86_64"
)
