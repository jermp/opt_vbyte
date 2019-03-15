#pragma once

#include "succinct/mapper.hpp"
#include "util.hpp"

using pvb::logger;

namespace pvb {

template <typename InputCollection, typename Collection>
void verify_collection(InputCollection const& input, const char* filename) {
    Collection coll;
    boost::iostreams::mapped_file_source m(filename);
    succinct::mapper::map(coll, m);

    logger() << "Checking index..." << std::endl;

    size_t size = 0;
    size_t seq_id = 0;

    for (auto seq : input) {
        size = seq.docs.size();

        auto& params = coll.params();
        params.blocks[0] = 0;
        params.blocks[1] = 0;

        auto e = coll[seq_id];
        if (e.size() != size) {
            logger() << "sequence " << seq_id << " has wrong length! ("
                     << e.size() << " != " << size << ")" << std::endl;
            exit(1);
        }

        for (size_t i = 0; i < e.size(); ++i, e.next()) {
            uint64_t docid = *(seq.docs.begin() + i);
            uint64_t freq = *(seq.freqs.begin() + i);

            if (docid != e.docid()) {
                logger() << "docid in sequence " << seq_id
                         << " differs at position " << i << "!" << std::endl;
                logger() << "got " << e.docid() << ", but expected " << docid
                         << std::endl;
                logger() << "sequence length: " << seq.docs.size() << std::endl;
                exit(1);
            }

            if (freq != e.freq()) {
                logger() << "freq in sequence " << seq_id
                         << " differs at position " << i << "!" << std::endl;
                logger() << "got " << e.freq() << ", but expected " << freq
                         << std::endl;
                logger() << "sequence length: " << seq.docs.size() << std::endl;
                exit(1);
            }
        }

        ++seq_id;
        if (seq_id % 1000000 == 0) {
            logger() << seq_id << " list checked" << std::endl;
        }

        // if (size > 2048) {
        //     std::cout << "{\"n\": " << size << ", \"vb\": "
        //               << params.blocks[0] * 100.0 /
        //                      (2 *
        //                       size)  // take into account both docs and freqs
        //               << ", \"rb\": " << params.blocks[1] * 100.0 / (2 *
        //               size)
        //               << "}" << std::endl;
        // }
    }

    logger() << seq_id << " list checked" << std::endl;
    logger() << "Everything is OK!" << std::endl;

    // auto& params = coll.params();
    // logger() << "sparse avg_gap " << params.sparse_avg_gap / params.blocks[0]
    //          << std::endl;
    // logger() << "dense avg_gap " << params.dense_avg_gap / params.blocks[1]
    //          << std::endl;
}
}  // namespace pvb
