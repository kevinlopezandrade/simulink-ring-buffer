# mmapblocks
Simulink s-functions to write and read from shared memory.

Before compiling the s-functions, ensure that the ncctools package is already
installed on your system, since the s-functions depend on the shared libraries
provided by ncctools. For details on how to install ncctools, please refer to
the README.md in the repository’s root directory.

## Building
Just run the command:
```bash
make
```
and the resulting .mex64 artifacts should be built.

## TODO
- Add unit tests in combination with simulink models.
- Currently only double types are allowed (extending this is not hard, but
better to pass it to C++ so that we can avoid long switch statements by using templates).
For enums the implementation is commented in the source code, but a similar treatment to other
types should be done.
