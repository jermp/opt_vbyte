#include <iostream>
#include <random>
#include <cassert>

#include "typedefs.hpp"
#include "uniform_partitioned_sequence.hpp"
#include "succinct/bit_vector.hpp"
#include "util.hpp"
#include "types.hpp"

#define M 1000000
#define B 1000000000

static const std::vector<uint64_t>
    jump_sizes = {   0,
                     1,   2,   4,    8,   16,   32,   64,
                   128, 256, 512, 1024
                   //, 2048, 4096, 8192
                 };

using namespace pvb;
typedef std::vector<uint64_t> uncompressed_sequence_type;

uncompressed_sequence_type
    random_sequence(uint64_t n, uint64_t max_gap)
{
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<uint64_t>
        dis(1, max_gap);

    uncompressed_sequence_type s;
    s.reserve(n);
    s.push_back(dis(gen));
    for (uint64_t i = 1; i < n; ++i) {
        s.push_back(s.back() + dis(gen));
    }

    assert(s.size() == n);
    assert(std::is_sorted(s.begin(), s.end()));

    uint64_t universe = s.back();
    uint64_t max_posting_bits = sizeof(posting_type) * 8;
    if (universe > (uint64_t(1) << max_posting_bits)) {
        std::cerr << "Universe too big to fit in "
                  << max_posting_bits << " bits.\n"
                  << "Reduce max_gap or the length of the list."
                  << std::endl;
        std::abort();
    }

    std::cerr << "\"avg_gap\": "
              << static_cast<double>(universe) / n << ", ";
    return s;
}

template<typename Enumerator>
void access_perf_test(Enumerator reader,
                      uncompressed_sequence_type const& s,
                      uint64_t runs)
{
    std::cout << "- access ------------------------------\n"
              << "[jump size] [num. queries] [ns x query]"
              << std::endl;

    // std::cerr << "\"timings\": [";

    uint64_t n = reader.size();

    for (uint64_t i = 0; i < jump_sizes.size(); ++i)
    {
        uint64_t jump_size = jump_sizes[i];
        auto start = clock_type::now();
        for (uint64_t run = 0; run < runs; ++run) {
            reader.move(0);
            assert(reader.docid() == s[0]);
            uint64_t j = 0;
            for (uint64_t i = jump_size; i < n; i += jump_size)
            {
                reader.move(i);
                do_not_optimize_away(reader.docid());
                assert(reader.docid() == s[i]);
                ++j;
                if (j == n) break;
            }
        }
        auto end = clock_type::now();
        std::chrono::duration<double> elapsed = end - start;
        uint64_t num_queries = (jump_size ? n / jump_size : n) * runs;
        double avg_time = elapsed.count() / num_queries  * B;
        std::cout << std::setw( 6) << jump_size
                  << std::setw(16) << num_queries
                  << std::setw(15) << std::fixed << std::setprecision(2)
                  << avg_time << std::endl;
        runs += 0.5 * runs; // perform more runs with less queries

        // std::cerr << avg_time;
        // if (i != jump_sizes.size() - 1) {
        //     std::cerr << ", ";
        // }
    }

    (void) s;
    // std::cerr << "]";
    // std::cout << "\n" << std::endl;
}

template<typename Enumerator>
void next_perf_test(Enumerator reader,
                    uncompressed_sequence_type const& s,
                    uint64_t runs)
{
    std::cout << "- next --------------------\n"
              << "[num. queries] [ns x query]"
              << std::endl;

    uint64_t n = reader.size();
    auto start = clock_type::now();

    for (uint64_t run = 0; run < runs; ++run) {
        reader.move(0);
        assert(reader.docid() == s[0]);
        for (uint64_t i = 1; i < n; ++i) {
            reader.next();
            do_not_optimize_away(reader.docid());
            assert(reader.docid() == s[i]);
        }
    }

    auto end = clock_type::now();
    std::chrono::duration<double> elapsed = end - start;
    std::cout << std::setw( 9) << n * runs
              << std::setw(16) << std::fixed << std::setprecision(2)
              << elapsed.count() / (n * runs)  * B
              << "\n" << std::endl;
    (void) s;
}

template<typename Enumerator>
void next_geq_perf_test(Enumerator reader,
                        uncompressed_sequence_type const& s,
                        uint64_t runs)
{
    std::cout << "- next_geq ----------------------------\n"
              << "[jump size] [num. queries] [ns x query]"
              << std::endl;

    std::cerr << "\"timings\": [";

    uint64_t n = reader.size();

    for (uint64_t i = 0; i < jump_sizes.size(); ++i)
    {
        uint64_t jump_size = jump_sizes[i];
        auto start = clock_type::now();
        for (uint64_t run = 0; run < runs; ++run) {
            reader.move(0);
            assert(reader.docid() == s[0]);
            uint64_t j = 0;
            for (uint64_t i = jump_size; i < n; i += jump_size)
            {
                reader.next_geq(s[i]);
                do_not_optimize_away(reader.docid());
                assert(reader.docid() == s[i]);
                ++j;
                if (j == n) break;
            }
        }
        auto end = clock_type::now();
        std::chrono::duration<double> elapsed = end - start;
        uint64_t num_queries = (jump_size ? n / jump_size : n) * runs;
        double avg_time = elapsed.count() / num_queries  * B;
        std::cout << std::setw( 6) << jump_size
                  << std::setw(16) << num_queries
                  << std::setw(15) << std::fixed << std::setprecision(2)
                  << avg_time << std::endl;
        runs += 0.5 * runs; // perform more runs with less queries

        std::cerr << avg_time;
        if (i != jump_sizes.size() - 1) {
            std::cerr << ", ";
        }
    }

    std::cerr << "]";
    std::cout << "\n" << std::endl;
}

template<typename BitSequenceType>
void bit_sequence_perf_test(uint64_t n, uint64_t max_gap, uint64_t runs)
{
    auto s = random_sequence(n, max_gap);

    global_parameters params;
    configuration conf(64);
    succinct::bit_vector_builder bvb;
    BitSequenceType::write(
        bvb, s.begin(), s.back() + 1, s.size(), params, conf
    );
    succinct::bit_vector bv(&bvb);

    typename BitSequenceType::enumerator
        reader(bv, 0, s.back() + 1, n, params);

    access_perf_test(reader, s, runs);
    next_perf_test(reader, s, runs);
    next_geq_perf_test(reader, s, runs);
}

template<typename ByteSequenceType>
void byte_sequence_perf_test(uint64_t n, uint64_t max_gap, uint64_t runs)
{
    auto s = random_sequence(n, max_gap);
    std::vector<uint8_t> data;
    ByteSequenceType::write(data, n, s.begin(), s.begin());

    typename ByteSequenceType::document_enumerator
        reader(data.data(), s.back() + 1);

    access_perf_test(reader, s, runs);
    next_perf_test(reader, s, runs);
    next_geq_perf_test(reader, s, runs);
}

int main (int argc, char** argv)
{
    if (argc < 4) {
        std::cerr << "Usage " << argv[0] << ":\n\t"
                  << "<sequence_type> <sequence_length> <max_gap>"
                  << std::endl;
        return 1;
    }

    std::string type = argv[1];
    uint64_t n = std::stoull(argv[2]);
    uint64_t max_gap = std::stoull(argv[3]);
    uint64_t runs = 5;

    std::cerr << "{";
    std::cerr << "\"sequence_type\": \"" << type << "\", ";
    std::cerr << "\"n\": " << n << ", ";
    std::cerr << "\"max_gap\": " << max_gap << ", ";

    if (type == std::string("vb")) {
        byte_sequence_perf_test<vb_sequence>(n, max_gap, runs);
    } else if (type == std::string("bic")) {
        byte_sequence_perf_test<bic_sequence>(n, max_gap, runs);
    } else if (type == std::string("uniform_vb")) {
        bit_sequence_perf_test<uniform1_vb_sequence>(n, max_gap, runs);
    } else if (type == std::string("uniform_rb")) {
        bit_sequence_perf_test<uniform_rb_sequence>(n, max_gap, runs);
    } else if (type == std::string("uniform_ef")) {
        bit_sequence_perf_test<uniform_ef_sequence>(n, max_gap, runs);
    } else {
        logger() << "ERROR: Unknown type " << type << std::endl;
    }

    std::cerr << "}" << std::endl;

    return 0;
}
