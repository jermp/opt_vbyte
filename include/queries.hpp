#pragma once

#include <iostream>
#include <sstream>

#include "types.hpp"
#include "queues.hpp"
#include "util.hpp"
#include "wand_data.hpp"

namespace ds2i {

    typedef uint32_t term_id_type;
    typedef std::vector<term_id_type> term_id_vec;

    bool read_query(term_id_vec &ret, std::istream &is = std::cin)
    {
        ret.clear();
        std::string line;
        if (!std::getline(is, line)) return false;
        std::istringstream iline(line);
        term_id_type term_id;

        while (iline >> term_id) {
            ret.push_back(term_id);
        }

        return true;
    }

    void remove_duplicate_terms(term_id_vec& terms) {
        std::sort(terms.begin(), terms.end());
        terms.erase(std::unique(terms.begin(), terms.end()), terms.end());
    }

    struct and_query {

        template<typename Index>
        uint64_t operator()(Index& index, term_id_vec terms) const
        {
            if (terms.empty()) {
                return 0;
            }
            remove_duplicate_terms(terms);

            typedef typename Index::document_enumerator enum_type;
            std::vector<enum_type> enums;
            enums.reserve(terms.size());

            for (auto term: terms) {
                enums.push_back(index[term]);
            }

            // sort by increasing frequency
            std::sort(enums.begin(), enums.end(),
                [](auto const& lhs, auto const& rhs) {
                    return lhs.size() < rhs.size();
                }
            );

            uint64_t results = 0;
            uint64_t candidate = enums[0].docid();
            size_t i = 1;
            while (candidate < index.num_docs()) {
                for (; i < enums.size(); ++i)
                {
                    enums[i].next_geq(candidate);
                    if (enums[i].docid() != candidate) {
                        candidate = enums[i].docid();
                        i = 0;
                        break;
                    }
                }

                if (i == enums.size()) {
                    results += 1;
                    enums[0].next();
                    candidate = enums[0].docid();
                    i = 1;
                }
            }

            return results;
        }
    };

    typedef std::pair<uint64_t, uint64_t> term_freq_pair;
    typedef std::vector<term_freq_pair> term_freq_vec;

    term_freq_vec query_freqs(term_id_vec terms) {
        term_freq_vec query_term_freqs;
        std::sort(terms.begin(), terms.end());
        // count query term frequencies
        for (size_t i = 0; i < terms.size(); ++i) {
            if (i == 0 || terms[i] != terms[i - 1]) {
                query_term_freqs.emplace_back(terms[i], 1);
            } else {
                query_term_freqs.back().second += 1;
            }
        }

        return query_term_freqs;
    }

    struct ranked_and_query {

        typedef bm25 scorer_type;
        typedef std::vector<scored_docid_type> scored_data_type;

        ranked_and_query(wand_data<scorer_type> const& wdata, uint64_t k)
            : m_wdata(&wdata)
            , m_topk(k)
        {}

        template<typename Index>
        uint64_t operator()(Index& index, term_id_vec terms) {

            typedef typename Index::document_enumerator enum_type;
            struct scored_enum {
                enum_type docs_enum;
                float q_weight;
            };

            m_topk.clear();
            if (terms.empty()) {
                return 0;
            }

            auto query_term_freqs = query_freqs(terms);
            std::vector<scored_enum> enums;
            enums.reserve(query_term_freqs.size());

            uint64_t num_docs = index.num_docs();
            for (auto term: query_term_freqs) {
                auto list = index[term.first];
                auto q_weight =
                    scorer_type::query_term_weight(term.second,
                                                   list.size(),
                                                   num_docs);
                enums.push_back(scored_enum{std::move(list), q_weight});
            }

            // sort by increasing frequency
            std::sort(enums.begin(), enums.end(),
                [](auto const &lhs, auto const &rhs) {
                    return lhs.docs_enum.size() < rhs.docs_enum.size();
                }
            );

            uint64_t candidate = enums[0].docs_enum.docid();
            size_t i = 1;
            while (candidate < index.num_docs()) {
                for (; i < enums.size(); ++i)
                {
                    enums[i].docs_enum.next_geq(candidate);
                    if (enums[i].docs_enum.docid() != candidate) {
                        candidate = enums[i].docs_enum.docid();
                        i = 0;
                        break;
                    }
                }

                if (i == enums.size()) {
                    float norm_len = m_wdata->norm_len(candidate);
                    float score = 0;
                    for (i = 0; i < enums.size(); ++i) {
                        score += enums[i].q_weight
                               * scorer_type::doc_term_weight(enums[i].docs_enum.freq(), norm_len)
                               ;
                    }

                    m_topk.insert(score, enums[0].docs_enum.docid());
                    enums[0].docs_enum.next();
                    candidate = enums[0].docs_enum.docid();
                    i = 1;
                }
            }

            m_topk.finalize();
            return m_topk.topk().size();
        }

        scored_data_type const& topk() const {
            return m_topk.topk();
        }

    private:
        wand_data<scorer_type> const* m_wdata;
        topk_queue<scored_data_type> m_topk;
    };
}
