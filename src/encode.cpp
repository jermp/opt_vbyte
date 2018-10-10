#include <iostream>
#include <algorithm>
#include <fstream>

#include <boost/preprocessor/seq/for_each.hpp>
#include <boost/preprocessor/stringize.hpp>
#include <boost/preprocessor/cat.hpp>
#include <boost/filesystem.hpp>
#include <boost/progress.hpp>

#include <succinct/mapper.hpp>

#include "opt_vbyte.hpp"

#include "util.hpp"
#include "binary_collection.hpp"
#include "queues.hpp"
#include "jobs.hpp"

using namespace pvb;

typedef binary_collection::posting_type const* iterator_type;
const uint32_t num_jobs = 1 << 24;

template<typename Iterator>
struct sequence_adder : semiasync_queue::job {
    sequence_adder(
        Iterator begin,
        uint64_t n, uint64_t universe,
        succinct::bit_vector_builder& bvb,
        boost::progress_display& progress,
        bool docs,
        uint64_t& num_processed_lists,
        uint64_t& num_total_ints
    )
        : begin(begin)
        , n(n)
        , universe(universe)
        , bvb(bvb)
        , progress(progress)
        , docs(docs)
        , num_processed_lists(num_processed_lists)
        , num_total_ints(num_total_ints)
    {}

    virtual void prepare()
    {
        opt_vbyte::encode(begin, universe, n, tmp, not docs);
    }

    virtual void commit()
    {
        progress += n + 1;
        ++num_processed_lists;
        num_total_ints += n;
        uint64_t offset = bvb.size() + tmp.size();
        bvb.append_bits(offset + 64 + 32 + 32, 64);
        bvb.append_bits(universe, 32);
        bvb.append_bits(n, 32);
        bvb.append(tmp);
    }

    Iterator begin;
    uint64_t n;
    uint64_t universe;
    succinct::bit_vector_builder& bvb;
    succinct::bit_vector_builder tmp;
    boost::progress_display& progress;
    bool docs;
    uint64_t& num_processed_lists;
    uint64_t& num_total_ints;
};

void save_if(char const* output_filename,
             std::vector<uint8_t> const& output)
{
    if (output_filename) {
        logger() << "writing encoded data..." << std::endl;
        std::ofstream output_file(output_filename);
        output_file.write(reinterpret_cast<char const*>(output.data()),
                          output.size() * sizeof(output[0]));
        output_file.close();
        logger() << "DONE" << std::endl;
    }
}

void print_statistics(std::string type, char const* collection_name,
                      std::vector<uint8_t> const& output,
                      uint64_t num_total_ints,
                      uint64_t num_processed_lists)
{
    double GB_space = output.size() * 1.0 / constants::GB;
    double bpi_space = output.size() * sizeof(output[0]) * 8.0 / num_total_ints;

    logger() << "encoded " << num_processed_lists << " lists" << std::endl;
    logger() << "encoded " << num_total_ints << " integers" << std::endl;
    logger() << GB_space << " [GB]" << std::endl;
    logger() << "bits x integer: " << bpi_space << std::endl;

    // stats to std output
    std::cout << "{";
    std::cout << "\"filename\": \"" << collection_name << "\", ";
    std::cout << "\"num_sequences\": \"" << num_processed_lists << "\", ";
    std::cout << "\"num_integers\": \"" << num_total_ints << "\", ";
    std::cout << "\"type\": \"" << type << "\", ";
    std::cout << "\"GB\": \"" << GB_space << "\", ";
    std::cout << "\"bpi\": \"" << bpi_space << "\"";
    std::cout << "}" << std::endl;
}

void encode(char const* collection_name,
            char const* output_filename)
{
    binary_collection input(collection_name);

    auto it = input.begin();
    uint64_t num_processed_lists = 0;
    uint64_t num_total_ints = 0;

    uint64_t total_progress = input.num_postings();
    bool docs = true;
    boost::filesystem::path collection_path(collection_name);
    if (collection_path.extension() == ".freqs") {
        docs = false;
        logger() << "encoding freqs..." << std::endl;
    } else if (collection_path.extension() == ".docs") {
        // skip first singleton sequence, containing num. of docs
        ++it;
        total_progress -= 2;
        logger() << "encoding docs..." << std::endl;
    } else {
        throw std::runtime_error("unsupported file format");
    }

    succinct::bit_vector_builder bvb;
    boost::progress_display progress(total_progress);
    semiasync_queue jobs_queue(num_jobs);

    for (; it != input.end(); ++it)
    {
        auto const& list = *it;
        uint32_t n = list.size();
        if (n > constants::min_size) {
            std::shared_ptr<sequence_adder<iterator_type>>
                ptr(new sequence_adder<iterator_type>(
                    list.begin(),
                    n, list.back() + 1, bvb,
                    progress, docs,
                    num_processed_lists, num_total_ints
                )
            );
            jobs_queue.add_job(ptr, n);
        }
    }

    jobs_queue.complete();

    double GB_space = (bvb.size() + 7.0) / 8.0 / constants::GB;
    double bpi_space = double(bvb.size()) / num_total_ints;

    logger() << "encoded " << num_processed_lists << " lists" << std::endl;
    logger() << "encoded " << num_total_ints << " integers" << std::endl;
    logger() << GB_space << " [GB]" << std::endl;
    logger() << "bits x integer: " << bpi_space << std::endl;

    // stats to std output
    std::cout << "{";
    std::cout << "\"filename\": \"" << collection_name << "\", ";
    std::cout << "\"num_sequences\": \"" << num_processed_lists << "\", ";
    std::cout << "\"num_integers\": \"" << num_total_ints << "\", ";
    std::cout << "\"type\": \"pef\", ";
    std::cout << "\"GB\": \"" << GB_space << "\", ";
    std::cout << "\"bpi\": \"" << bpi_space << "\"";
    std::cout << "}" << std::endl;

    if (output_filename) {
        logger() << "writing encoded data..." << std::endl;
        succinct::bit_vector bv(&bvb);
        succinct::mapper::freeze(bv, output_filename);
        logger() << "DONE" << std::endl;
    }
}

int main(int argc, char** argv) {

    if (argc < 3) {
        std::cerr << "Usage " << argv[0] << ":\n"
                  << "\t<collection_name> [--out <output_filename>]"
                  << std::endl;
        return 1;
    }

    char const* collection_name = argv[1];
    char const* output_filename = nullptr;

    for (int i = 2; i < argc; ++i) {
        if (argv[i] == std::string("--out")) {
            ++i;
            output_filename = argv[i];
        } else {
            throw std::runtime_error("unknown parameter");
        }
    }

    encode(collection_name, output_filename);

    return 0;
}
