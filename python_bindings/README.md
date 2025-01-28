# Python Bindings to NCCTools
## Installation

Before installing these Python bindings, ensure that the ncctools package is
already installed on your system. These bindings rely on the shared libraries
provided by ncctools. For details on how to install ncctools, please refer to
the README.md in the repository’s root directory.

Once ncctools is installed, install these Python bindings in developer mode by running:
```bash
pip install -e .
```
Developer mode is typically sufficient for most development and testing purposes.

## Examples
You can find usage examples in the examples/ directory.
Some examples require additional Python dependencies:

- cbor2
- FastApi

Install them as needed ideally in your custom enviroment (e.g conda) before running the
corresponding examples.
