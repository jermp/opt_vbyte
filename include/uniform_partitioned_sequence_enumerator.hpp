#pragma once

namespace pvb {

template <typename Encoder, typename UpperBounds>
struct uniform_partitioned_sequence_enumerator {
    typedef typename Encoder::enumerator base_sequence_enumerator;
    typedef typename UpperBounds::enumerator upper_bounds_enumerator;

    uniform_partitioned_sequence_enumerator() {}

    uniform_partitioned_sequence_enumerator(succinct::bit_vector const& bv,
                                            uint64_t offset, uint64_t universe,
                                            uint64_t n,
                                            global_parameters& params)
        : m_params(&params)
        , m_size(n)
        , m_universe(universe)
        , m_position(0)
        , m_bv(&bv) {
        succinct::bit_vector::enumerator it(bv, offset);
        m_partitions = read_gamma_nonzero(it);

        if (m_partitions == 1) {
            m_cur_partition = 0;
            m_cur_begin = 0;
            m_cur_end = n;

            uint64_t universe_bits = ceil_log2(universe);
            m_cur_base = it.take(universe_bits);
            auto ub = 0;
            if (n > 1) {
                uint64_t universe_delta = read_delta(it);
                ub = universe_delta ? universe_delta
                                    : (universe - m_cur_base - 1);
            }

            eat_pad(it, alignment);

            m_partition_enumerator = base_sequence_enumerator(
                *m_bv, it.position(), ub + 1, n, *m_params);
            m_cur_upper_bound = m_cur_base + ub;

        } else {
            m_endpoint_bits = read_gamma(it);
            UpperBounds::align(it);
            uint64_t cur_offset = it.position();

            m_upper_bounds_enumerator = upper_bounds_enumerator(
                bv, cur_offset, universe, m_partitions + 1, *m_params);

            cur_offset += typename UpperBounds::offsets(
                              0, universe, m_partitions + 1, *m_params)
                              .end;

            m_endpoints_offset = cur_offset;
            uint64_t endpoints_size = m_endpoint_bits * (m_partitions - 1);
            cur_offset += endpoints_size;

            m_sequences_offset = cur_offset;

            uint64_t mod = m_sequences_offset % alignment;
            if (mod) {
                uint64_t pad = alignment - mod;
                m_sequences_offset += pad;
            }
            assert(m_sequences_offset % alignment == 0);

            slow_move();
        }
    }

    pv_type DS2I_ALWAYSINLINE move(uint64_t position) {
        assert(position <= size());
        m_position = position;

        if (m_position >= m_cur_begin and m_position < m_cur_end) {
            m_value =
                m_cur_base +
                m_partition_enumerator.move(m_position - m_cur_begin).second;
            return value();
        }

        return slow_move();
    }

    pv_type DS2I_ALWAYSINLINE next_geq(uint64_t lower_bound) {
        if (DS2I_LIKELY(lower_bound >= m_cur_base and
                        lower_bound <= m_cur_upper_bound)) {
            auto val =
                m_partition_enumerator.next_geq(lower_bound - m_cur_base);
            m_position = m_cur_begin + val.first;
            m_value = m_cur_base + val.second;
            return value();
        }
        return slow_next_geq(lower_bound);
    }

    pv_type DS2I_ALWAYSINLINE next() {
        ++m_position;
        if (DS2I_LIKELY(m_position < m_cur_end)) {
            m_value = m_cur_base + m_partition_enumerator.next().second;
            return value();
        }
        return slow_next();
    }

    uint64_t prev_value() const {
        if (DS2I_UNLIKELY(m_position == m_cur_begin)) {
            return m_cur_partition ? m_cur_base - 1 : 0;
        } else {
            return m_cur_base + m_partition_enumerator.prev_value();
        }
    }

    uint64_t size() const {
        return m_size;
    }

    uint64_t docid() const {
        return m_value;
    }

    uint64_t position() const {
        return m_position;
    }

private:
    inline pv_type value() const {
        return pv_type(m_position, m_value);
    }

    pv_type DS2I_NOINLINE slow_next() {
        if (DS2I_UNLIKELY(m_position == m_size)) {
            assert(m_cur_partition == m_partitions - 1);
            auto val = m_partition_enumerator.next();
            assert(val.first == m_partition_enumerator.size());
            (void)val;
            m_value = m_universe;
            return value();
        }

        switch_partition(m_cur_partition + 1);
        m_value = m_cur_base + m_partition_enumerator.move(0).second;
        return value();
    }

    pv_type DS2I_NOINLINE slow_move() {
        if (m_position == size()) {
            if (m_partitions > 1) {
                switch_partition(m_partitions - 1);
            }
            m_partition_enumerator.move(m_partition_enumerator.size());
            m_value = m_universe;
            return value();
        }
        uint64_t partition = m_position >> m_params->log_partition_size;
        switch_partition(partition);
        m_value = m_cur_base +
                  m_partition_enumerator.move(m_position - m_cur_begin).second;
        return value();
    }

    pv_type DS2I_NOINLINE slow_next_geq(uint64_t lower_bound) {
        if (m_partitions == 1) {
            if (lower_bound < m_cur_base) {
                return move(0);
            } else {
                return move(size());
            }
        }

        auto ub_it = m_upper_bounds_enumerator.next_geq(lower_bound);

        if (ub_it.first == 0) {
            return move(0);
        }

        if (ub_it.first == m_upper_bounds_enumerator.size()) {
            return move(size());
        }

        switch_partition(ub_it.first - 1);
        return next_geq(lower_bound);
    }

    void switch_partition(uint64_t partition) {
        assert(m_partitions > 1);

        uint64_t endpoint =
            partition ? m_bv->get_bits(m_endpoints_offset +
                                           (partition - 1) * m_endpoint_bits,
                                       m_endpoint_bits)
                      : 0;
        m_bv->data().prefetch((m_sequences_offset + endpoint) / 64);

        m_cur_partition = partition;
        m_cur_begin = partition << m_params->log_partition_size;
        m_cur_end =
            std::min(size(), (partition + 1) << m_params->log_partition_size);

        auto ub_it = m_upper_bounds_enumerator.move(partition + 1);
        m_cur_upper_bound = ub_it.second;
        m_cur_base =
            m_upper_bounds_enumerator.prev_value() + (partition ? 1 : 0);

        m_partition_enumerator =
            base_sequence_enumerator(*m_bv, m_sequences_offset + endpoint,
                                     m_cur_upper_bound - m_cur_base + 1,
                                     m_cur_end - m_cur_begin, *m_params);
    }

    global_parameters* m_params;
    uint64_t m_partitions;
    uint64_t m_endpoints_offset;
    uint64_t m_endpoint_bits;
    uint64_t m_sequences_offset;
    uint64_t m_size;
    uint64_t m_universe;

    uint64_t m_position;
    uint64_t m_value;

    uint64_t m_cur_partition;
    uint64_t m_cur_begin;
    uint64_t m_cur_end;
    uint64_t m_cur_base;
    uint64_t m_cur_upper_bound;

    succinct::bit_vector const* m_bv;
    upper_bounds_enumerator m_upper_bounds_enumerator;
    base_sequence_enumerator m_partition_enumerator;
};

}  // namespace pvb
