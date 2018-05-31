opt_vbyte
=========

This is the code used for the experiments in the paper [*Variable-Byte Encoding is Now Space-Efficient Too*](http://pages.di.unipi.it/pibiri/papers/VByte18.pdf)[1], by Giulio Ermanno Pibiri and Rossano Venturini.

This guide is meant to provide a brief overview of the library and to illustrate its funtionalities through some examples.
##### Table of contents
* [Building the code](#building-the-code)
* [Input data format](#input-data-format)
* [Building the indexes](#building-the-indexes)
* [Authors](#authors)
* [Bibliography](#bibliography)

Building the code
-----------------

The code is tested on Linux Ubuntu with `gcc` 5.4.1. The following dependencies are needed for the build: `CMake` >= 2.8 and `Boost` >= 1.58.

The code is largely based on the [`ds2i`](https://github.com/ot/ds2i) project, so it depends on several submodules. If you have cloned the repository without `--recursive`, you will need to perform the following commands before
building:

    $ git submodule init
    $ git submodule update

To build the code on Unix systems (see file `CMakeLists.txt` for the used compilation flags), it is sufficient to do the following:

    $ mkdir build
    $ cd build
    $ cmake .. -DCMAKE_BUILD_TYPE=Release
    $ make -j[number of jobs]

Setting `[number of jobs]` is recommended, e.g., `make -j4`.

Unless otherwise specified, for the rest of this guide we assume that we type the terminal commands of the following examples from the created directory `build`.


Input data format
-----------------
The collection containing the docID and frequency lists follow the format of [`ds2i`](https://github.com/ot/ds2i), that is all integer lists are prefixed by their length written as 32-bit little-endian unsigned integers:

* `<basename>.docs` starts with a singleton binary sequence where its only
  integer is the number of documents in the collection. It is then followed by
  one binary sequence for each posting list, in order of term-ids. Each posting
  list contains the sequence of docIDs containing the term.

* `<basename>.freqs` is composed of a one binary sequence per posting list, where
  each sequence contains the occurrence counts of the postings, aligned with the
  previous file (note however that this file does not have an additional
  singleton list at its beginning).

** Describe the content of the subfolder `data' **

For the following examples, we assume to work with the sample data contained in `data`.

Building the indexes
--------------------

<!-- The executables `create_clustered_freq_index_fb` (frequency-based) and `create_clustered_freq_index_sb` (space-based) can be used to build clustered Elias-Fano indexes, given an input collection and a set of clusters.
For the other parameters of the executables, see the corresponding `.cpp` files. Below we show some examples.

##### Example 1.
The command

    $ ./create_clustered_freq_index_fb ../test_data/test_collection.bin \
    ../test_data/test_collection.clusters.gz 800000 clustered_opt_index.800K.bin

builds a clustered Elias-Fano index:
* using the frequency-based approach;
* whose reference list size is 800,000;
* that is serialized to the binary file `clustered_opt_index.800K.bin`.

##### Example 2.
The command

    $ ./create_freq_index opt ../test_data/test_collection.bin \
    --clusters ../test_data/test_collection.clusters.gz opt_index.bin

builds a partitioned Elias-Fano index on the same postings lists used by the corresponding clustered index (see Example 1.), as specified with the option `--clusters` and serialized to the binary file `opt_index.bin`.

##### Example 3.
The command

    $ ./create_freq_index block_interpolative ../test_data/test_collection.bin \
    --clusters ../test_data/test_collection.clusters.gz bic_index.bin

builds a Binary Interpolative index on the same postings lists used by the corresponding clustered index (see Example 1.), as specified with the option `--clusters` and serialized to the binary file `bic_index.bin`.


A comparison between the space of such indexes is summarized by the following table, where CPEF indicates the clustered Elias-Fano index, PEF the partitioned Elias-Fano index and BIC the Binary Interpolative one.

|     **Index**     |**bits x posting** |
|-------------------|-------------------|
|CPEF               |4.23               |
|PEF                |5.15 (**+17.86%**) |
|BIC                |4.60 (**+8.04%**)  | -->

Authors
-------
* [Giulio Ermanno Pibiri](http://pages.di.unipi.it/pibiri/), <giulio.pibiri@di.unipi.it>
* [Rossano Venturini](http://pages.di.unipi.it/rossano/), <rossano.venturini@unipi.it>

Bibliography
------------
* [1] Giulio Ermanno Pibiri and Rossano Venturini, *Variable-Byte Encoding is Now Space-Efficient Too*. CoRR 2018.
