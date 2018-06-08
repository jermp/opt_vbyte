#include <algorithm>
#include <fstream>
#include <iostream>
#include <numeric>
#include <thread>

#include <boost/algorithm/string/predicate.hpp>
#include <boost/optional.hpp>

#include "succinct/mapper.hpp"

#include "bm25.hpp"
#include "configuration.hpp"
#include "index_build_utils.hpp"
#include "types.hpp"
#include "util.hpp"
#include "verify_collection.hpp"

using namespace pvb;
using pvb::logger;

template <typename Collection>
void dump_index_specific_stats(Collection const &, std::string const &)
{}

template<typename InputCollection, typename CollectionType, typename Scorer = bm25>
void create_collection(InputCollection const& input,
                       global_parameters const& params,
                       configuration const& conf,
                       const char* output_filename,
                       bool check,
                       std::string const& seq_type)
{
    logger() << "Building index with F = " << conf.fix_cost << std::endl;
    logger() << "Processing " << input.num_docs() << " documents" << std::endl;
    double tick = get_time_usecs();
    double user_tick = get_user_time_usecs();

    typename CollectionType::builder builder(input.num_docs(), params);
    progress_logger plog;
    uint64_t size = 0;

    for (auto const& plist: input) {
        size = plist.docs.size();
        uint64_t freqs_sum = 0;
        freqs_sum = std::accumulate(plist.freqs.begin(), plist.freqs.begin() + size, uint64_t(0));
        builder.add_posting_list(size, plist.docs.begin(), plist.freqs.begin(), freqs_sum, conf);
        plog.done_sequence(size);
    }

    plog.log();
    CollectionType coll;
    builder.build(coll);
    double elapsed_secs = (get_time_usecs() - tick) / 1000000;
    double user_elapsed_secs = (get_user_time_usecs() - user_tick) / 1000000;

    logger() << seq_type << " collection built in " << elapsed_secs << " seconds" << std::endl;
    stats_line()("type", seq_type)("worker_threads", conf.worker_threads)(
        "construction_time", elapsed_secs)("construction_user_time", user_elapsed_secs)
    ;

    dump_stats(coll, seq_type, plog.postings);

    if (output_filename) {
        logger() << "saving index on disk" << std::endl;
        double tick = get_time_usecs();
        succinct::mapper::freeze(coll, output_filename);
        double elapsed_secs = (get_time_usecs() - tick) / 1000000;
        logger() << "done in " << elapsed_secs << " seconds" << std::endl;

        if (check) {
            verify_collection<InputCollection, CollectionType>(input, output_filename);
        }
    }
}

int main(int argc, char** argv) {

    if (argc < 3) {
        std::cerr << "Usage " << argv[0] << ":\n"
                  << "\t<index_type> <collection_basename> [--out <output_filename>] [--F <fix_cost>] [--check]"
                  << std::endl;
        return 1;
    }

    std::string type = argv[1];
    const char* collection_basename = argv[2];
    const char* output_filename = nullptr;
    uint64_t F = 64;
    bool check = false;

    for (int i = 3; i < argc; ++i) {
        if (argv[i] == std::string("--out")) {
            output_filename = argv[++i];
        } else if (argv[i] == std::string("--F")) {
            F = std::stoull(argv[++i]);
        } else if (argv[i] == std::string("--check")) {
            check = true;
        } else {
            std::cerr << "Unknown parameter" << std::endl;
            return 1;
        }
    }

    binary_freq_collection input(collection_basename);

    configuration conf(F);
    global_parameters params;
    params.log_partition_size = conf.log_partition_size;

    if (false) {
#define LOOP_BODY(R, DATA, T)                                               \
    }                                                                       \
    else if (type == BOOST_PP_STRINGIZE(T)) {                               \
        create_collection<binary_freq_collection, BOOST_PP_CAT(T, _index)>(           \
            input, params, conf, output_filename, check, type/*, K, encode_all*/);    \

        BOOST_PP_SEQ_FOR_EACH(LOOP_BODY, _, DS2I_INDEX_TYPES);
#undef LOOP_BODY
    } else {
        logger() << "ERROR: Unknown type " << type << std::endl;
    }

    return 0;
}
