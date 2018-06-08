#pragma once

#include <vector>
#include <algorithm>
#include <iterator>

#include "util.hpp"
#include "typedefs.hpp"
#include "indexed_sequence.hpp"

namespace pvb {

    typedef uint64_t cost_t;

    template<typename Encoder>
    struct optimal_partition {

        std::vector<posting_type> partition;
        cost_t cost_opt = 0;

        template<typename ForwardIterator>
        struct cost_window {
            // a window reppresents the cost of the interval [start, end)
            ForwardIterator start_it;
            ForwardIterator end_it;

            // starting and ending position of the window
            posting_type start = 0;
            posting_type end   = 0; // one-past the end
            posting_type min_p = 0; // element that preceeds the first element of the window
            posting_type max_p = 0;

            cost_t cost_upper_bound;  // max cost for this window
            cost_t third_cost = 0;    // cost for this window as encoded with third compressor (not all_ones, nor ranked_bitvector)

            cost_window(ForwardIterator begin, posting_type base, cost_t cost_upper_bound,
                        global_parameters const& params)
                : start_it(begin)
                , end_it(begin)
                , min_p(base)
                , max_p(base)
                , cost_upper_bound(cost_upper_bound)
                , third_cost(0)
                , m_begin(begin)
                , m_params(params)
            {}

            uint64_t universe() const {
                return max_p - min_p + 1;
            }

            uint64_t size() const {
                return end - start;
            }

            void advance_start()
            {
                uint64_t posting_cost =
                    Encoder::posting_cost(*start_it, min_p - 1);
                third_cost -= posting_cost;
                min_p = *start_it + 1;
                ++start;
                ++start_it;
            }

            void advance_end()
            {
                uint64_t posting_cost =
                    Encoder::posting_cost(*end_it, max_p);
                third_cost += posting_cost;
                max_p = *end_it;
                ++end;
                ++end_it;
            }

            uint64_t cost() const {
                return indexed_sequence<Encoder>::bitsize(m_params, third_cost,
                                                          universe(), size());
            }

        private:
            ForwardIterator m_begin;
            global_parameters const& m_params;
            uint64_t m_base;
        };

        optimal_partition()
        {}

        // (1 + eps) approximation
        template<typename ForwardIterator, typename CostFunction>
        optimal_partition(ForwardIterator begin,
                          posting_type base, posting_type universe, uint64_t size,
                          CostFunction cost_fun, double eps1, double eps2,
                          global_parameters const& params, uint64_t fix_cost)
        {
            cost_t single_block_cost = cost_fun(begin, universe - base, size);
            // std::cout << "single_block_cost " << single_block_cost << std::endl;
            std::vector<cost_t> min_cost(size + 1, single_block_cost);
            min_cost[0] = 0;

            std::vector<cost_window<ForwardIterator>> windows;

            // create the required window: one for each power of approx_factor
            cost_t cost_lb = cost_fun(begin, 1, 1); // minimum cost
            cost_t cost_bound = cost_lb;
            while (eps1 == 0 or cost_bound < cost_lb / eps1) {
                windows.emplace_back(begin, base, cost_bound, params);
                if (cost_bound >= single_block_cost) break;
                cost_bound = cost_bound * (1 + eps2);
            }

            std::vector<posting_type> path(size + 1, 0);
            for (posting_type i = 0; i < size; i++) {
                size_t last_end = i + 1;
                for (auto& window: windows) {

                    assert(window.start == i);
                    while (window.end < last_end) {
                        window.advance_end();
                    }

                    cost_t window_cost = 0;
                    while (true) {
                        // window_cost = cost_fun(window.start_it, window.universe(), window.size());
                        window_cost = window.cost() + fix_cost;

                        if (min_cost[i] + window_cost < min_cost[window.end]) {
                            min_cost[window.end] = min_cost[i] + window_cost;
                            path[window.end] = i;
                        }
                        last_end = window.end;
                        if (window.end == size) break;
                        if (window_cost >= window.cost_upper_bound) break; // ignore this for quadratic solution
                        window.advance_end();
                    }

                    window.advance_start();
                }
            }

            posting_type curr_pos = size;
            while (curr_pos != 0) {
                partition.push_back(curr_pos);
                curr_pos = path[curr_pos];
            }
            std::reverse(partition.begin(), partition.end());
            cost_opt = min_cost[size];

            // std::cout << "num. partitions " << partition.size() << std::endl;
            // std::cout << "sizes:cost\n";
            // for (auto s: partition) {
            //     std::cout << s << ":" << min_cost[s] << std::endl;
            //     // std::cout << min_cost[s] << std::endl;
            // }
            // std::cout << "opt_cost " << cost_opt << std::endl;
            // std::cout << "bit x doc " << cost_opt * 1.0 / size << std::endl;
        }

        // exact quadratic solution for debugging on small files
        // template<typename ForwardIterator, typename CostFunction>
        // optimal_partition(ForwardIterator begin,
        //                   posting_type base, posting_type universe, uint64_t size,
        //                   CostFunction cost_fun, double eps1, double eps2,
        //                   global_parameters const& params, uint64_t fix_cost)
        // {
        //     (void) eps1;
        //     (void) eps2; // solution is exact
        //     cost_t single_block_cost = cost_fun(begin, universe - base, size);
        //     std::vector<cost_t> min_cost(size + 1, single_block_cost);
        //     min_cost[0] = 0;

        //     std::vector<cost_window<ForwardIterator>> windows;
        //     windows.reserve(size + 1);
        //     for (uint64_t i = 0; i < size + 1; ++i) {
        //         windows.emplace_back(begin, base, 0, params);
        //     }

        //     std::vector<posting_type> path(size + 1, 0);
        //     for (uint64_t i = 0; i < size; ++i) {
        //         for (uint64_t j = i + 1; j < size + 1; ++j) {
        //             auto& window = windows[j];
        //             assert(window.start == i);

        //             while (window.end < j) {
        //                 window.advance_end();
        //             }
        //             assert(window.end == j); // one-past the end
        //             cost_t window_cost = window.cost() + fix_cost;
        //             if (min_cost[i] + window_cost < min_cost[j]) {
        //                 min_cost[j] = min_cost[i] + window_cost;
        //                 path[j] = i;
        //             }

        //             window.advance_start();
        //         }
        //     }

        //     posting_type curr_pos = size;
        //     while (curr_pos != 0) {
        //         partition.push_back(curr_pos);
        //         curr_pos = path[curr_pos];
        //     }
        //     std::reverse(partition.begin(), partition.end());
        //     cost_opt = min_cost[size];

        //     // std::cout << "num. partitions " << partition.size() << std::endl;
        //     // std::cout << "sizes:cost\n";
        //     // for (auto s: partition) {
        //     //     // std::cout << s << ":" << min_cost[s] << std::endl;
        //     //     std::cout << min_cost[s] << std::endl;
        //     // }
        //     // if (partition.size() == 1) {
        //     //     cost_opt -= fix_cost;
        //     // }
        //     // std::cout << "opt_cost " << cost_opt << std::endl;
        //     // std::cout << "bit x doc " << cost_opt * 1.0 / size << std::endl;
        // }
    };
}
