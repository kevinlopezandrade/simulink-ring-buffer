# NCCTools
## Serialization Dependency: TinyCBOR

For serialization, we use CBOR (Concise Binary Object Representation) and the
Intel implementation TinyCBOR. This library is a required dependency for our
project. Although it would be ideal to compile it as a static library and
manage it with Bazel, this approach isn't straightforward since TinyCBOR does
not provide a BUILD file. While it might be possible to create one, we prefer
to avoid introducing complex workarounds to keep the project clean and
maintainable.

Additionally, TinyCBOR is a useful library to have available system-wide
because mmapblocks also requires it. Therefore, we recommend building and
installing it manually into the system.

By default, the library installs into `/usr/local`. Installing here requires root
permissions. If you have the necessary permissions, you can build and install
TinyCBOR as follows:
```bash
cd tinycbor
make
make install
```

This will place the TinyCBOR shared libraries in `/usr/local/lib` and the headers
in `/usr/local/include/tinycbor`.


## Installing NCCTools
We use Bazel as our build system. To build all targets, simply run:
```bash
bazel build //...
```
This command compiles all Bazel targets and creates a bazel-bin directory
(symlink) containing the resulting artifacts. Among these artifacts, you should
see a .deb package. To install it, use:
```
dpkg -i {package-name}.deb 
```
The above command will install the ncctools shared libraries under `/usr/lib` and
the headers in `/usr/include/ncctools`.

## Structure
`ncctools/`: Contains the code and headers for the implementation of the ring buffer.
`python_bindings/`: Provides the Python library that exposes bindings to the ring buffer.
`mmapblocks/`: Includes S-functions for reading from and writing to shared memory.

### Troubleshooting & Debugging

If the linker cannot locate libtinycbor.so, check whether your Linux system
includes `/usr/local/lib` in its library search paths. On Ubuntu, you can verify
this by inspecting `/etc/ld.so.conf` and its included configuration files.

To resolve missing paths:
1. Add `/usr/local/lib` to your system's library search paths if it's missing.
For example, edit `/etc/ld.so.conf` or add the path in a new file under `/etc/ld.so.conf.d/`.
2. Run the following command to update the dynamic linker cache:
```bash
ldconfig
```

To verify that the library is properly detected, use:
```bash
ldconfig -p | grep "tinycbor"
```
If the installation was successful, the shared library should appear in the output.

The same steps can be done to check if the libncctools-ringbuffer.so is installed
properly in the system.
