#include "succinct/mapper.hpp"

#include "types.hpp"
#include "util.hpp"

#include <chrono>

using namespace pvb;
using pvb::logger;
using pvb::get_time_usecs;
using pvb::do_not_optimize_away;

typedef std::chrono::high_resolution_clock clock_type;

template<typename IndexType>
void perftest(IndexType& index, std::string const& type)
{
    {
        size_t min_length =  500000;
        size_t max_length = 5000000;
        size_t max_number_of_lists = 5000;

        std::vector<size_t> long_lists;
        long_lists.reserve(max_number_of_lists);
        logger() << "warming up posting lists" << std::endl;
        for (size_t i = 0; i < index.size() and long_lists.size() <= max_number_of_lists; ++i) {
            if (index[i].size() >= min_length and index[i].size() < max_length) {
                long_lists.push_back(i);
                index.warmup(i);
            }
        }

        logger() << "Scanning " << long_lists.size()
                 << " posting lists, whose length is between "
                 << min_length << " and " << max_length << std::endl;

        auto start = clock_type::now();
        size_t postings = 0;
        for (auto i: long_lists) {
            auto reader = index[i];
            size_t size = reader.size();
            for (size_t i = 0; i < size; ++i) {
                reader.next();
                do_not_optimize_away(reader.docid());
            }
            postings += size;
        }
        auto end = clock_type::now();
        std::chrono::duration<double> elapsed = end - start;

        double next_ns = elapsed.count() / postings * 1000000000;
        logger() << "Performed " << postings << " next()"
                 << " in " << elapsed.count() << " [sec], "
                 << std::fixed << std::setprecision(2)
                 << next_ns << " [ns] x posting"
                 << std::endl;

        std::cout << type << "\t" << "next"
                  << "\t" << next_ns << std::endl;
    }
}

template<typename IndexType>
void perftest(const char* index_filename, std::string const& type)
{
    logger() << "Loading index from " << index_filename << std::endl;
    IndexType index;
    boost::iostreams::mapped_file_source m(index_filename);
    succinct::mapper::map(index, m, succinct::mapper::map_flags::warmup);
    perftest<IndexType>(index, type);
}

int main(int argc, const char** argv) {

    if (argc < 3) {
        std::cerr << "Usage: " << argv[0] << ":\n"
                  << "\t <index_type> <index_filename>"
                  << std::endl;
        return 1;
    }

    std::string index_type = argv[1];
    const char* index_filename = argv[2];

    if (false) {
#define LOOP_BODY(R, DATA, T)                               \
        } else if (index_type == BOOST_PP_STRINGIZE(T)) {   \
            perftest<BOOST_PP_CAT(T, _index)>               \
                (index_filename, index_type);               \

        BOOST_PP_SEQ_FOR_EACH(LOOP_BODY, _, DS2I_INDEX_TYPES);
#undef LOOP_BODY
    } else {
        logger() << "ERROR: Unknown index type '" << index_type << "'." << std::endl;
        return 1;
    }

    return 0;
}
