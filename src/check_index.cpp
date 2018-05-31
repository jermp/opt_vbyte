#include <fstream>
#include <iostream>

#include "configuration.hpp"
#include "index_build_utils.hpp"
#include "types.hpp"
#include "util.hpp"
#include "verify_collection.hpp"

int main(int argc, char** argv) {

    if (argc < 6) {
        std::cerr << "Usage " << argv[0] << ":\n"
                  << "\t<index_type> <index_filename> <collection_filename> <length_1> <length_2> [--print_statistics]"
                  << std::endl;
        std::cerr << "\n-----------------\n";
        std::cerr << "If a posting list has size in between <length_1> and <length_2>,\n"
                  << "it is taken into account for computing the avg_gap of the collection."
                  << std::endl;
        return 1;
    }

    using namespace ds2i;
    std::string index_type = argv[1];
    const char* index_filename = argv[2];
    const char* collection_filename = argv[3];
    uint64_t l1 = std::stoull(argv[4]);
    uint64_t l2 = std::stoull(argv[5]);

    bool print_statistics = false;
    if (argc > 6 and argv[6] == std::string("--print_statistics")) {
        print_statistics = true;
    }

    binary_freq_collection input(collection_filename);

    if (false) {
#define LOOP_BODY(R, DATA, T)                                                \
    }                                                                        \
    else if (index_type == BOOST_PP_STRINGIZE(T)) {                          \
        verify_collection<binary_freq_collection, BOOST_PP_CAT(T, _index)>(  \
            input, index_filename, print_statistics, l1, l2);                \

        BOOST_PP_SEQ_FOR_EACH(LOOP_BODY, _, DS2I_INDEX_TYPES);
#undef LOOP_BODY
    } else {
        logger() << "ERROR: Unknown type " << index_type << std::endl;
    }

    return 0;
}
