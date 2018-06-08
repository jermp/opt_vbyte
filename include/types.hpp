#pragma once

#include <boost/preprocessor/cat.hpp>
#include <boost/preprocessor/seq/for_each.hpp>
#include <boost/preprocessor/stringize.hpp>

#include "binary_freq_collection.hpp"
#include "block_codecs.hpp"
#include "block_freq_index.hpp"
#include "freq_index.hpp"

#include "partitioned_sequence.hpp"
#include "partitioned_vb_sequence.hpp"

#include "positive_sequence.hpp"
#include "indexed_sequence.hpp"
#include "uniform_partitioned_sequence.hpp"
#include "block_sequence.hpp"

#include "uncompressed_upper_bounds.hpp"
#include "compact_ranked_bitvector.hpp"

namespace pvb {

    /* Partitioned VByte indexes */

    typedef uniform_partitioned_sequence<
                indexed_sequence<block_sequence<maskedvbyte_block>>,
                // uncompressed_upper_bounds
                compact_elias_fano
            > uniform2_vb_sequence;

    typedef uniform_partitioned_sequence<
                block_sequence<maskedvbyte_block>,
                // uncompressed_upper_bounds
                compact_elias_fano
            > uniform1_vb_sequence;

    using uniform_vb_index =
        freq_index<
            uniform2_vb_sequence,
            positive_sequence<
                uniform2_vb_sequence
            >
        >;

    // solution with dynamic programming
    using opt_vb_dp_index =
        freq_index<
            partitioned_sequence<
                block_sequence<maskedvbyte_block>
            >,
            positive_sequence<
                partitioned_sequence<
                    block_sequence<maskedvbyte_block>
                >
            >
        >;

    // solution with scan
    using opt_vb_index =
        freq_index<
            partitioned_vb_sequence<maskedvbyte_block>,
            positive_sequence<
                partitioned_vb_sequence<maskedvbyte_block>
            >
        >;

    /* Unpartitioned VByte indexes */
    using block_streamvbyte_index = block_freq_index<streamvbyte_block>;
    using block_maskedvbyte_index = block_freq_index<maskedvbyte_block>;
    using block_varintg8iu_index  = block_freq_index<varintg8iu_block>;
    using block_varintgb_index    = block_freq_index<varintgb_block>;

    /* Benchmark and test sequences */
    using vb_sequence =
        block_posting_list<maskedvbyte_block>;

    using bic_sequence =
        block_posting_list<interpolative_block>;

    using uniform_ef_sequence =
        uniform_partitioned_sequence<
            compact_elias_fano,
            compact_elias_fano
            // uncompressed_upper_bounds
        >;

    using uniform_rb_sequence =
        uniform_partitioned_sequence<
            compact_ranked_bitvector,
            compact_elias_fano
            // uncompressed_upper_bounds
        >;
}

#define DS2I_INDEX_TYPES                                                               \
    (block_varintg8iu)(block_streamvbyte)(block_maskedvbyte)(block_varintgb)           \
    (uniform_vb)(opt_vb_dp)(opt_vb)

#define DS2I_BLOCK_INDEX_TYPES                                                         \
    (block_streamvbyte)(block_maskedvbyte)(block_varintg8iu)(block_varintgb)
