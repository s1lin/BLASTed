BLASTed
=======

A collection of sparse matrix containers and manipulators, along with certain linear algebra operations. Note that this is not a complete sparse BLAS library; instead, the focus is on providing thread-parallel low-level kernels required to implement preconditioners for large sparse linear systems of algebraic equations, in an object-oriented framework. In case of a distributed-memory parallel solver, the operations implemented here are meant to be used in the subdomains' local preconditioners. 

The main contribution of this library is a practical implementation of asynchronous iterations as preconditioners. These include scalar and point-block versions of ['asynchronous symmetric Gauss-Seidel' iterations](https://www.sciencedirect.com/science/article/pii/S0045793018308429) developed as part of this work, an independent implementation of an [asynchronous incomplete LU factorization preconditioner](https://epubs.siam.org/doi/10.1137/140968896), as well as an asynchronous point-block ILU factorization preconditioner with asynchronous block-triangular solves. The advantages of point-block asynchronous ILU preconditioners, especially for compressible fluid flow problems, are documented in [this preprint submitted to SIAM](https://arxiv.org/abs/1912.00539).

Building
--------
The following programs and libraries are required:
- [CMake](https://cmake.org/) version 3.0 or later
- [Eigen](http://eigen.tuxfamily.org) version 3.3.4 or later (an even more recent version is needed for the GCC 7 series) [`EIGEN3_ROOT`]
- [Boost](http://www.boost.org/) [`BOOST_ROOT`]
- [PETSc](http://www.mcs.anl.gov/petsc/) 3.8 or above and MPI are required to build the PETSc interface. [`PETSC_DIR`, `PETSC_ARCH`] Note that for the release build of BLASTed, PETSc must be configured with `--with-memalign=64` on x86\_64 platforms.

Quick-build:
Assuming the programs and libraries mentioned above are in standard locations, in your `PATH` or `LD_LIBRARY_PATH` environment variables, or you have otherwise made them available by setting the environment variables mentioned in square brackets above, `cd` to the top-level directory of the BLASTed sources and try:

    mkdir build && cd build
	../build_example.sh
	make -j4
	make test

If this does not work, you may need to run cmake manually and change some settings.

Long version: Assuming that you are in the top-level BLASTed directory, to configure a release version with AVX vectorization and the PETSc interface, type

    mkdir build && cd build
	cmake -DAVX=1 -DWITH_PETSC=1 -DCMAKE_C_COMPILER=mpicc -DCMAKE_CXX_COMPILER=mpicxx -DCMAKE_BUILD_TYPE=Release ..

where `mpicc` and `mpicxx` are the C and C++ MPI compiler wrappers you want to use. This will build the library for use with PETSc with available block sizes 4 and 5 (the default block sizes). To build without the PETSc interface, `-DWITH_PETSC` should be removed. `-DBUILD_BLOCK_SIZE=10` can be specified to additionally build the block solver operations for a block size of 10, for instance. See the beginning of the top-level CMakeLists.txt file for all the options. To build,

    make -j4

and to run the tests,

	make test

A C++ compiler with C++ 14 support is required. The build is known to work with GCC 7.3, 8.1, 8.2, 9.1, Intel 2017, 2018, and Clang 6.0, 7.0 in a GNU/Linux environment. To build in other enviroments, tweaking the CMakeLists.txt file will be required. This build process generates a shared object `build/lib/libblasted_petsc.so` that should be linked to your application.

To build the [Doxygen](http://www.stack.nl/~dimitri/doxygen/) documentation,

    cd path/to/BLASTed/doc
    doxygen blasted_doxygen.cfg

This will build HTML documentation in a subdirectory called html in the current directory.

Finally, from the build directory, one can issue

    make tags
   
to generate a tags file for [easier navigation of the source code in Vim](http://vim.wikia.com/wiki/Browsing_programs_with_tags). If you are using [Spacemacs](http://spacemacs.org), this is not needed; just use the shortcut `SPC p G`.

Usage
-----
For the most part, one would want to use the library as a plugin for PETSc. This is explained in the [usage documentation](doc/user-doc.md).

---

[![Built with Spacemacs](https://cdn.rawgit.com/syl20bnr/spacemacs/442d025779da2f62fc86c2082703697714db6514/assets/spacemacs-badge.svg)](http://spacemacs.org)
