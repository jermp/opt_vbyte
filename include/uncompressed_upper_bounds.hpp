#pragma once

#include "typedefs.hpp"

namespace ds2i {

    struct uncompressed_upper_bounds {
        uncompressed_upper_bounds()
        {}

        static void align(succinct::bit_vector_builder& bvb) {
            push_pad(bvb);
            assert(bvb.size() % 8 == 0);
        }

        static void align(succinct::bit_vector::enumerator& it) {
            eat_pad(it);
            assert(it.position() % 8 == 0);
        }

        struct offsets {
            offsets()
            {}

            offsets(uint64_t base_offset,
                    uint64_t universe, uint64_t n,
                    global_parameters const& params)
                : end(sizeof(posting_type) * 8 * n)
            {
                (void) base_offset;
                (void) universe;
                (void) params;
                assert(n > 0);
            }

            uint64_t end;
        };

        template<typename Iterator>
        static void write(succinct::bit_vector_builder& bvb,
                          Iterator begin,
                          uint64_t universe, uint64_t n,
                          global_parameters const& params)
        {
            (void) universe;
            (void) params;
            uint64_t upper_bound_bits = sizeof(posting_type) * 8;
            for (uint64_t i = 0; i < n; ++i, ++begin) {
                bvb.append_bits(*begin, upper_bound_bits);
            }
            assert(bvb.size() % 8 == 0);
        }

        struct enumerator {

            enumerator()
            {}

            enumerator(succinct::bit_vector const& bv, uint64_t offset,
                       uint64_t universe, uint64_t n,
                       global_parameters const& params)
                : m_position(0)
                , m_value(0)
                , m_universe(universe)
                , m_n(n)
            {
                (void) params;
                auto addr = reinterpret_cast<uint8_t const*>(&(bv.data()[0]));
                m_ptr = reinterpret_cast<posting_type const*>(addr + offset / 8);
                move(0);
            }

            uint64_t size() const {
                return m_n;
            }

            pv_type move(uint64_t position) {
                assert(position <= size());
                m_position = position;
                if (DS2I_UNLIKELY(m_position == size())) {
                    m_value = m_universe;
                } else {
                    m_value = *(m_ptr + m_position);
                }
                return pv_type(m_position, m_value);
            }

            uint64_t prev_value() const {
                if (DS2I_UNLIKELY(m_position == 0)) {
                    return 0;
                }
                return *(m_ptr + m_position - 1);
            }

            pv_type next_geq(uint64_t lower_bound)
            {
                if (lower_bound < *m_ptr) { // overflow from left
                    return pv_type(0, *m_ptr);
                }

                assert(lower_bound >= m_value or m_position == 0);

                auto tmp = m_ptr + m_position;
                while (m_value < lower_bound and m_position < size()) {
                    ++m_position;
                    ++tmp;
                    m_value = *tmp;
                }

                if (m_position == size()) {
                    m_value = m_universe;
                }

                return pv_type(m_position, m_value);
            }

            // debug
            void print() const {
                std::cout << "upper_bounds:\n";
                for (uint64_t i = 0; i < size(); ++i) {
                    std::cout << *(m_ptr + i) << " ";
                }
            }

        private:
            uint64_t m_position;
            uint64_t m_value;
            uint64_t m_universe;
            uint64_t m_n;
            posting_type const* m_ptr;
        };
    };
}
