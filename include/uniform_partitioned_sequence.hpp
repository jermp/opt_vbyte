#pragma once

#include <stdexcept>

#include "compact_elias_fano.hpp"
#include "global_parameters.hpp"
#include "configuration.hpp"
#include "integer_codes.hpp"
#include "util.hpp"
#include "uniform_partitioned_sequence_enumerator.hpp"

namespace ds2i {

    template<typename Encoder, typename UpperBounds>
    struct uniform_partitioned_sequence {

        typedef Encoder base_sequence_type;
        typedef uniform_partitioned_sequence_enumerator<
                    base_sequence_type,
                    UpperBounds
                > enumerator;

        template<typename Iterator>
        static void write(succinct::bit_vector_builder& bvb,
                          Iterator begin,
                          uint64_t universe,
                          uint64_t n,
                          global_parameters const& params,
                          configuration const& /*conf*/)
        {
            using succinct::util::ceil_div;
            assert(n > 0);
            uint64_t partition_size = uint64_t(1) << params.log_partition_size;
            size_t partitions = ceil_div(n, partition_size);
            write_gamma_nonzero(bvb, partitions);

            std::vector<uint64_t> cur_partition;
            uint64_t cur_base = 0;
            if (partitions == 1) {
                cur_base = *begin;
                Iterator it = begin;

                for (size_t i = 0; i < n; ++i, ++it) {
                    cur_partition.push_back(*it - cur_base);
                }

                uint64_t universe_bits = ceil_log2(universe);
                bvb.append_bits(cur_base, universe_bits);
                // write universe only if non-singleton and not tight
                if (n > 1) {
                    if (cur_base + cur_partition.back() + 1 == universe) {
                        // tight universe
                        write_delta(bvb, 0);
                    } else {
                        write_delta(bvb, cur_partition.back());
                    }
                }

                push_pad(bvb, alignment);
                base_sequence_type::write(
                    bvb,
                    cur_partition.begin(),
                    cur_partition.back() + 1,
                    cur_partition.size(), params
                );

            } else {
                succinct::bit_vector_builder bv_sequences;
                std::vector<uint64_t> endpoints;
                std::vector<uint64_t> upper_bounds;

                uint64_t cur_i = 0;
                Iterator it = begin;
                cur_base = *begin;
                upper_bounds.push_back(cur_base);

                for (size_t p = 0; p < partitions; ++p) {
                    cur_partition.clear();
                    uint64_t value = 0;
                    for (; cur_i < ((p + 1) * partition_size) and cur_i < n; ++cur_i, ++it) {
                        value = *it;
                        cur_partition.push_back(value - cur_base);
                    }
                    assert(cur_partition.size() <= partition_size);
                    assert((p == partitions - 1) or cur_partition.size() == partition_size);

                    uint64_t upper_bound = value;
                    assert(cur_partition.size() > 0);

                    base_sequence_type::write(bv_sequences,
                                              cur_partition.begin(),
                                              cur_partition.back() + 1,
                                              cur_partition.size(), params);
                    endpoints.push_back(bv_sequences.size());
                    upper_bounds.push_back(upper_bound);
                    cur_base = upper_bound + 1;
                }

                succinct::bit_vector_builder bv_upper_bounds;
                UpperBounds::write(bv_upper_bounds, upper_bounds.begin(),
                                   universe, partitions + 1, params);

                uint64_t endpoint_bits = ceil_log2(bv_sequences.size() + 1);
                write_gamma(bvb, endpoint_bits);
                UpperBounds::align(bvb);
                bvb.append(bv_upper_bounds);

                for (uint64_t p = 0; p < endpoints.size() - 1; ++p) {
                    bvb.append_bits(endpoints[p], endpoint_bits);
                }

                push_pad(bvb, alignment);
                bvb.append(bv_sequences);
            }
        }
    };
}
