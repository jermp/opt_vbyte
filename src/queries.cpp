#include <iostream>

#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/lexical_cast.hpp>

#include "succinct/mapper.hpp"

#include "types.hpp"
#include "queries.hpp"
#include "util.hpp"

using namespace ds2i;

template<typename Functor>
void op_perftest(Functor query_func,
                 std::vector<ds2i::term_id_vec> const& queries,
                 std::string const& index_type,
                 std::string const& query_type,
                 size_t runs)
{
    std::vector<double> query_times;

    for (size_t run = 0; run <= runs; ++run) {
        for (auto const& query: queries) {
            auto tick = get_time_usecs();
            uint64_t result = query_func(query);
            do_not_optimize_away(result);
            double elapsed = double(get_time_usecs() - tick);
            if (run != 0) { // first run is not timed
                query_times.push_back(elapsed);
            }
        }
    }

    if (false) {
        for (auto t: query_times) {
            std::cout << (t / 1000) << std::endl;
        }
    } else {
        std::sort(query_times.begin(), query_times.end());
        double avg = std::accumulate(query_times.begin(),
                                     query_times.end(), double(0))
                                   / query_times.size();

        logger() << "---- " << index_type << " " << query_type << std::endl;
        logger() << "---- performed " << query_times.size() << " queries" << std::endl;
        avg /= 1000;
        logger() << "Mean: " << avg << " [ms]" << std::endl;
        stats_line()("type", index_type)("query", query_type)("avg", avg);
    }
}

template<typename IndexType>
void perftest(const char* index_filename,
              const char* wand_data_filename,
              std::vector<ds2i::term_id_vec> const& queries,
              std::string const& index_type,
              std::string const& query_type,
              uint64_t k)
{
    IndexType index;
    logger() << "Loading index from " << index_filename << std::endl;
    boost::iostreams::mapped_file_source m(index_filename);
    succinct::mapper::map(index, m);

    logger() << "Warming up posting lists" << std::endl;
    std::unordered_set<term_id_type> warmed_up;
    for (auto const& q: queries) {
        for (auto t: q) {
            if (!warmed_up.count(t)) {
                index.warmup(t);
                warmed_up.insert(t);
            }
        }
    }

    wand_data<> wdata;
    boost::iostreams::mapped_file_source md;
    if (wand_data_filename) {
        logger() << "Loading wand data from " << wand_data_filename << std::endl;
        md.open(wand_data_filename);
        succinct::mapper::map(wdata, md, succinct::mapper::map_flags::warmup);
    }

    logger() << "Performing " << query_type << " queries" << std::endl;

    std::function<uint64_t(ds2i::term_id_vec)> query_fun;

    if (query_type == "and") {
        query_fun = [&](ds2i::term_id_vec query) {
            return and_query()(index, query);
        };
    } else if (query_type == "ranked_and") {
        if (wand_data_filename) {
            logger() << "top-" << k << " results" << std::endl;
            query_fun = [&](ds2i::term_id_vec query) {
                return ranked_and_query(wdata, k)(index, query);
            };
        } else {
            logger() << "You must provide wand data to perform ranked_and queries." << std::endl;
            return;
        }
    } else {
        logger() << "Unsupported query type: " << query_type << std::endl;
        return;
    }

    op_perftest(query_fun, queries, index_type, query_type, 2);
}

int main(int argc, const char** argv) {
    using namespace ds2i;

    if (argc < 3) {
        std::cerr << "Usage: " << argv[0] << ":\n"
                  << "\t <index_type> <query_algorithm> <index_filename> <query_filename> "
                  << "[--profile] [--k K] [--wand wand_filename]"
                  << std::endl;
        return 1;
    }

    std::string index_type = argv[1];
    std::string query_type = argv[2];
    const char* index_filename = argv[3];
    const char* query_filename = argv[4];

    configuration conf(64);
    uint64_t k = conf.k;
    const char* wand_data_filename = nullptr;

    for (int i = 5; i < argc; ++i)
    {
        std::string arg = argv[i];

        if (arg == "--wand") {
            wand_data_filename = argv[++i];
        }

        if (arg == "--k") {
            k = std::stoull(argv[++i]);
        }
    }

    std::vector<term_id_vec> queries;
    term_id_vec q;

    {
        std::filebuf fb;
        if (fb.open(query_filename, std::ios::in)) {
            std::istream is(&fb);
            while (read_query(q, is)) {
                queries.push_back(q);
            }
        }
    }

    if (false) {
#define LOOP_BODY(R, DATA, T)                                  \
    }                                                          \
    else if (index_type == BOOST_PP_STRINGIZE(T)) {            \
        perftest<BOOST_PP_CAT(T, _index)>(                     \
            index_filename, wand_data_filename, queries,       \
            index_type, query_type, k                          \
        );                                                     \

        BOOST_PP_SEQ_FOR_EACH(LOOP_BODY, _, DS2I_INDEX_TYPES);
#undef LOOP_BODY

    } else {
        logger() << "ERROR: Unknown index type '" << index_type << "'." << std::endl;
        return 1;
    }

    return 0;
}
