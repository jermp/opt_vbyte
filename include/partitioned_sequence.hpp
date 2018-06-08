#pragma once

#include <stdexcept>

#include "optimal_partition.hpp"
#include "partitioned_sequence_enumerator.hpp"

namespace pvb {

    template<typename Encoder>
    struct partitioned_sequence {

        typedef indexed_sequence<Encoder> base_sequence_type;
        typedef partitioned_sequence_enumerator<
                    base_sequence_type
                > enumerator;

        template <typename Iterator>
        static void write(succinct::bit_vector_builder& bvb,
                          Iterator begin,
                          uint64_t universe, uint64_t n,
                          global_parameters const& params,
                          configuration const& conf)
        {
            assert(n > 0);

            auto partition = compute_partition(begin, universe, n, params, conf);

            size_t partitions = partition.size();
            assert(partitions > 0);
            assert(partition.front() != 0);
            assert(partition.back() == n);

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
                push_pad(bvb);
                base_sequence_type::write(bvb, cur_partition.begin(),
                                          cur_partition.back() + 1,
                                          cur_partition.size(),
                                          params);
            } else {
                succinct::bit_vector_builder bv_sequences;
                std::vector<uint64_t> endpoints;
                std::vector<uint64_t> upper_bounds;

                uint64_t cur_i = 0;
                Iterator it = begin;
                cur_base = *begin;
                upper_bounds.push_back(cur_base);

                for (size_t p = 0; p < partition.size(); ++p) {
                    cur_partition.clear();
                    uint64_t value = 0;
                    for (; cur_i < partition[p]; ++cur_i, ++it) {
                        value = *it;
                        cur_partition.push_back(value - cur_base);
                    }

                    uint64_t upper_bound = value;
                    assert(cur_partition.size() > 0);
                    base_sequence_type::write(bv_sequences, cur_partition.begin(),
                                              cur_partition.back() + 1,
                                              cur_partition.size(), // XXX skip last one?
                                              params);
                    endpoints.push_back(bv_sequences.size());
                    upper_bounds.push_back(upper_bound);
                    cur_base = upper_bound + 1;
                }

                succinct::bit_vector_builder bv_sizes;
                succinct::bit_vector_builder bv_upper_bounds;
                compact_elias_fano::write(bv_sizes, partition.begin(),
                                          n, partitions - 1, params);
                compact_elias_fano::write(bv_upper_bounds, upper_bounds.begin(),
                                          universe, partitions + 1, params);
                uint64_t endpoint_bits = ceil_log2(bv_sequences.size() + 1);
                write_gamma(bvb, endpoint_bits);

                bvb.append(bv_sizes);
                bvb.append(bv_upper_bounds);

                for (uint64_t p = 0; p < endpoints.size() - 1; ++p) {
                    bvb.append_bits(endpoints[p], endpoint_bits);
                }

                push_pad(bvb);
                bvb.append(bv_sequences);
            }
        }

    private:

        template <typename Iterator>
        static std::vector<uint32_t>
        compute_partition(Iterator begin,
                          uint64_t universe, uint64_t n,
                          global_parameters const& params,
                          configuration const& conf)
        {
            std::vector<uint32_t> partition;

            auto cost_fun = [&](auto begin, uint64_t universe, uint64_t n) {
                return base_sequence_type::bitsize(begin, params, universe, n) + conf.fix_cost;
            };

            const size_t superblock_bound =
                conf.eps3 != 0
                ? size_t(conf.fix_cost / conf.eps3)
                : n;

            std::deque<std::vector<uint32_t>> superblock_partitions;
            task_region(*conf.executor, [&](task_region_handle& thr) {
                size_t superblock_pos = 0;
                auto superblock_begin = begin;
                auto superblock_base = *begin;

                while (superblock_pos < n) {
                    size_t superblock_size = std::min<size_t>(superblock_bound,
                                                              n - superblock_pos);
                    // If the remainder is smaller than the bound (possibly
                    // empty), merge it to the current (now last) superblock.
                    if (n - (superblock_pos + superblock_size) < superblock_bound) {
                        superblock_size = n - superblock_pos;
                    }
                    auto superblock_last = std::next(superblock_begin, superblock_size - 1);
                    auto superblock_end = std::next(superblock_last);

                    // If this is the last superblock, its universe is the
                    // list universe.
                    size_t superblock_universe =
                        superblock_pos + superblock_size == n
                        ? universe
                        : *superblock_last + 1;

                    superblock_partitions.emplace_back();
                    auto& superblock_partition = superblock_partitions.back();

                    thr.run([=, &cost_fun, &conf, &superblock_partition] {
                        optimal_partition<Encoder>
                            opt(superblock_begin,
                                superblock_base,
                                superblock_universe,
                                superblock_size,
                                cost_fun, conf.eps1, conf.eps2,
                                params, conf.fix_cost);

                        superblock_partition.reserve(opt.partition.size());
                        for (auto& endpoint: opt.partition) {
                            superblock_partition.push_back(superblock_pos + endpoint);
                        }
                    });

                    superblock_pos += superblock_size;
                    superblock_begin = superblock_end;
                    superblock_base = superblock_universe;
                }
            });

            for (const auto& superblock_partition: superblock_partitions) {
                partition.insert(partition.end(),
                                 superblock_partition.begin(),
                                 superblock_partition.end());
            }

            return partition;
        }
    };
}
