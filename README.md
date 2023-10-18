# PLATO Compression Library

__TL;DR__ If you don't know what this is, you probably don't want to.

This is the hardware compressor control and software compression library for the PLATO ASW.

### Hardware Compressor

The FPGA-based SpaceWire Router and Data Compressor Unit (RDCU) was developed
in collaboration with the University of Vienna, Department of Astrophysics
(IFA/UVIE) and the Austrian Academy of Sciences Space Research Institute
(IWF/OEAW) for the PLATO mission.

The RDCU consists of a GR718RB SpaceWire router and an RTAX2000 FPGA
holding the RMAP Target and Data Compressor.

We provide an interface library for configuring the SpaceWire router and
controlling the hardware compressor. A demonstrator of this can be found in the
[`examples`](examples) directory.

### Software Compressor

The University of Vienna team provides a set of compression algorithms in the
form of a static library to assist the Instrument Control Unit (ICU) in
compressing the various PLATO data products.

If you need a CLI for compressing or decompressing data, use the
[cmp_tool](https://gitlab.phaidra.org/loidoltd15/cmp_tool).

## Build instructions

We only support a Unix-like build environment.  Running `make` in the root
directory will create the static library `libcmp.a` in the `lib` directory. In
this setup, the library contains the software compression, hardware control and
decompression.

Other available options include:

- `make test`: create and run all unit tests
- `make coverage`: create an coverage report of all unit tests
- `make all`: build all tests and examples

For advanced use cases, specialized compilation flags which control binary generation
are documented in [`lib/README.md`](lib/README.md#modular-build).
