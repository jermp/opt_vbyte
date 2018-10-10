#pragma once

#include "block_codecs.hpp"

namespace pvb {

    struct opt_vbyte {
        typedef partitioned_vb_sequence<maskedvbyte_block> docs_sequence_type;
        typedef positive_sequence<
                    partitioned_sequence<maskedvbyte_block>
                > freqs_sequence_type;

        static void encode(uint32_t const* in,
                           uint32_t universe, uint32_t n,
                           succinct::bit_vector_builder& bvb,
                           bool freqs)
        {
            static const global_parameters params;
            if (freqs) {
                freqs_sequence_type::write(bvb, in, universe, n, params);
            } else {
                docs_sequence_type::write(bvb, in, universe, n, params);
            }
        }

        static void decode(succinct::bit_vector const& bv,
                           uint32_t* out, uint64_t offset,
                           uint64_t universe, uint64_t n,
                           bool freqs)
        {
            if (freqs) {
                freqs_sequence_type::decode(bv, out, offset, universe, n);
            } else {
                docs_sequence_type::decode(bv, out, offset, universe, n);
            }
        }
    };

}