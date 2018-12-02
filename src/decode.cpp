#include <iostream>
#include <fstream>
#include <algorithm>

#include <boost/preprocessor/seq/for_each.hpp>
#include <boost/preprocessor/stringize.hpp>
#include <boost/preprocessor/cat.hpp>
#include <boost/filesystem.hpp>
#include <boost/iostreams/device/mapped_file.hpp>

#include <sys/mman.h>

#include <succinct/mapper.hpp>

#include "opt_vbyte.hpp"
#include "util.hpp"

using namespace pvb;

void print_statistics(std::string type,
                      char const* encoded_data_filename,
                      std::vector<double> const& timings,
                      uint64_t num_decoded_ints,
                      uint64_t num_decoded_lists
                      )
{
    double tot_elapsed = std::accumulate(timings.begin(), timings.end(), double(0.0));
    double ns_x_int = tot_elapsed * 1000000000 / num_decoded_ints;
    uint64_t ints_x_sec = uint64_t(1 / ns_x_int * 1000000000);

    logger() << "elapsed time " << tot_elapsed << " [sec]" << std::endl;
    logger() << ns_x_int << " [ns] x int" << std::endl;
    logger() << ints_x_sec << " ints x [sec]" << std::endl;

    // stats to std output
    std::cout << "{";
    std::cout << "\"filename\": \"" << encoded_data_filename << "\", ";
    std::cout << "\"num_sequences\": \"" << num_decoded_lists << "\", ";
    std::cout << "\"num_integers\": \"" << num_decoded_ints << "\", ";
    std::cout << "\"type\": \"" << type << "\", ";
    std::cout << "\"tot_elapsed_time\": \"" << tot_elapsed << "\", ";
    std::cout << "\"ns_x_int\": \"" << ns_x_int << "\", ";
    std::cout << "\"ints_x_sec\": \"" << ints_x_sec << "\"";
    std::cout << "}" << std::endl;
}

void decode(char const* encoded_data_filename, bool freqs)
{
    succinct::bit_vector bv;
    boost::iostreams::mapped_file_source m(encoded_data_filename);
    succinct::mapper::map(bv, m);
    uint64_t num_bits = bv.size();

    std::vector<uint32_t> decoded;
    decoded.resize(constants::max_size, 0);
    std::vector<double> timings;

    uint64_t num_decoded_ints = 0;
    uint64_t num_decoded_lists = 0;
    uint64_t offset = 0;
    uint64_t universe, n;

    logger() << "decoding..." << std::endl;
    while (offset < num_bits) {

        uint64_t next_offset = bv.get_bits(offset, 64);
        offset += 64;
        universe = bv.get_bits(offset, 32);
        offset += 32;
        n = bv.get_bits(offset, 32);
        offset += 32;

        assert(offset % alignment == 0);

        auto start = clock_type::now();
        opt_vbyte::decode(
            bv, decoded.data(), offset, universe, n, freqs
        );
        auto finish = clock_type::now();
        std::chrono::duration<double> elapsed = finish - start;
        timings.push_back(elapsed.count());
        num_decoded_ints += n;
        ++num_decoded_lists;
        offset = next_offset;
    }

    std::cout << "num_decoded_ints " << num_decoded_ints << std::endl;
    double tot_elapsed = std::accumulate(timings.begin(), timings.end(), double(0.0));
    double ns_x_int = tot_elapsed * 1000000000 / num_decoded_ints;
    uint64_t ints_x_sec = uint64_t(1 / ns_x_int * 1000000000);

    logger() << "elapsed time " << tot_elapsed << " [sec]" << std::endl;
    logger() << ns_x_int << " [ns] x int" << std::endl;
    logger() << ints_x_sec << " ints x [sec]" << std::endl;
}

int main(int argc, char** argv) {

    if (argc < 2) {
        std::cerr << "Usage " << argv[0] << ":\n"
                  << "\t<encoded_data_filename> [--freqs]"
                  << std::endl;
        return 1;
    }

    char const* encoded_data_filename = argv[1];
    bool freqs = false;

    for (int i = 2; i < argc; ++i) {
        if (argv[i] == std::string("--freqs")) {
            freqs = true;
            ++i;
        } else {
            throw std::runtime_error("unknown parameter");
        }
    }

    decode(encoded_data_filename, freqs);

    return 0;
}
