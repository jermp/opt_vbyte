opt_vbyte
=========

This is the code used for the experiments in the paper [*Variable-Byte Encoding is Now Space-Efficient Too*](http://pages.di.unipi.it/pibiri/papers/VByte18.pdf) [1], by Giulio Ermanno Pibiri and Rossano Venturini.

This guide is meant to provide a brief overview of the library and to illustrate its functionalities through some examples.
##### Table of contents
* [Building the code](#building-the-code)
* [Input data format](#input-data-format)
* [Building the indexes](#building-the-indexes)
* [Benchmark](#benchmark)
* [Authors](#authors)
* [Bibliography](#bibliography)

Building the code
-----------------

The code is tested on Linux Ubuntu with `gcc` 7.3.0. The following dependencies are needed for the build: `CMake` >= 2.8 and `Boost` >= 1.42.0.

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

The `data` subfolder contains an example of such collection organization, for a total of 113,306 sequences and 3,327,520 postings. The `queries` file is, instead, a collection of 500 (multi-term) queries.

For the following examples, we assume to work with the sample data contained in `data`.

Building the indexes
--------------------

The executables `src/create_freq_index` should be used to build the indexes, given an input collection. To know the parameters needed by the executable, just type

    $ ./create_freq_index

without any parameters. You will get:

    $ Usage ./create_freq_index:
    $       <index_type> <collection_basename> [--out <output_filename>] [--F <fix_cost>] [--check]

Below we show some examples.

##### Example 1.
The command

    $ ./create_freq_index opt_vb ../data/test_collection --out test.opt_vb.bin

builds an optimally-partitioned VByte index that is serialized to the binary file `test.opt_vb.bin`.

##### Example 2.
The command

    $ ./create_freq_index block_maskedvbyte ../data/test_collection --out test.vb.bin

builds an un-partitioned VByte index that is serialized to the binary file `test.vb.bin`, using [`Masked-VByte`](https://github.com/lemire/MaskedVByte.git) to perform sequential decoding.

##### Example 3.
The command

    $ ./queries opt_vb and test.opt_vb.bin ../data/queries

performed the boolean AND queries contained in the data file `queries` over the index serialized to `test.opt_vb.bin`.

Benchmark
---------

A comparison between the space of un-partitioned VByte and partitioned VByte indexes (uniform, eps-optimal and optimal) is shown below (`bpi` stands for "bits per integer"). Results have been collected on a machine with an i7-4790K processor clocked at 4GHz and running Linux 4.13.0 (Ubuntu 17.10), 64 bits. The code was compiled using the highest optimization setting.

|     **Index**     |**docs [bpi]**  |**freqs [bpi]**  |**building time [secs]**| **µsec/query** |
|-------------------|---------------:|----------------:|-----------------------:|---------------:|
|VByte              |10.498          | 8.031           |    0.704               |   4.316        |
|VByte uniform      | 8.118          | 4.686           |    0.769               |   4.339        |
|VByte eps-optimal  | 7.438          | 4.302           |    3.419               |   4.434        |
|VByte optimal      | 7.388          | 4.268           |    0.739               |   4.378        |

Authors
-------
* [Giulio Ermanno Pibiri](http://pages.di.unipi.it/pibiri/), <giulio.pibiri@di.unipi.it>
* [Rossano Venturini](http://pages.di.unipi.it/rossano/), <rossano.venturini@unipi.it>

Bibliography
------------
* [1] Giulio Ermanno Pibiri and Rossano Venturini, *Variable-Byte Encoding is Now Space-Efficient Too*. CoRR 2018.
