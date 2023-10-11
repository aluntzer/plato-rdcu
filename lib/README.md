PLATO Compression Library Files
===============================

The __lib__ directory is divided into several subdirectories. This makes it
easier to select or exclude features.

### Building

A `Makefile` script is provided to build the compression library.

- `make`: create an optimised static library
- `make lib`: create a static library with debugging flags
- `make lib-release`: same as `make`
- `make clean`: remove any build artefacts in the lib directory

### API

- The software/ICU compression API is exposed within [lib/cmp_icu.h](cmp_icu.h).
- The hardware/RDCU compression API is exposed within [lib/cmp_rdcu.h](cmp_rdcu.h).
- The decompression API is exposed within [lib/decmp.h](decmp.h).

### Modular build

It's possible to compile only a limited set of features within `libcmp.a`.

- The `lib/common` directory is always required for all variants
- Software/ICU compression source code lies in `lib/icu_compress`
- Hardware/RDCU Compression source code lies in `lib/rdcu_compress`
- Decompression source code lies in `lib/decompress`

- It is possible to include only `icu_compress`, `rdcu_compress` or
  `decompress` or a combination of the components, they are not dependent on
   each other.
- While invoking `make` or `make lib`, it's possible to define build macros
  `LIB_ICU_COMPRESSION`, `LIB_RDCU_COMPRESSION`, `LIB_DECOMPRESSION`, as `0`
  to forgo compilation of the corresponding features. For example,
  `make LIB_RDCU_COMPRESSION=0 LIB_DECOMPRESSION=0` will only include the
  software compression source without the hardware and decompression part.
