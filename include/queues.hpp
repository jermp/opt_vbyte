#pragma once

#include <thread>
#include <memory>
#include <deque>

#include "configuration.hpp"
#include "util.hpp"
#include "typedefs.hpp"

namespace pvb {

class semiasync_queue {
public:
    semiasync_queue(double work_per_thread, configuration const& conf)
        : m_expected_work(0), m_work_per_thread(work_per_thread) {
        m_max_threads = conf.worker_threads;
        // logger() << "semiasync_queue using " << m_max_threads
        //          << " worker threads" << std::endl;
    }

    class job {
    public:
        virtual void prepare() = 0;
        virtual void commit() = 0;
        virtual ~job() = default;
    };

    typedef std::shared_ptr<job> job_ptr_type;

    void add_job(job_ptr_type j, double expected_work) {
        if (m_max_threads) {
            m_next_thread.first.push_back(j);
            m_expected_work += expected_work;
            if (m_expected_work >= m_work_per_thread) {
                spawn_next_thread();
            }
        } else {  // all in main thread
            j->prepare();
            j->commit();
            j.reset();
        }
    }

    void complete() {
        if (!m_next_thread.first.empty()) {
            spawn_next_thread();
        }
        while (!m_running_threads.empty()) {
            commit_thread();
        }
    }

private:
    void spawn_next_thread() {
        if (m_running_threads.size() == m_max_threads) {
            commit_thread();
        }

        m_running_threads.emplace_back();
        std::swap(m_next_thread, m_running_threads.back());

        std::vector<job_ptr_type> const& cur_queue =
            m_running_threads.back().first;
        m_running_threads.back().second = std::thread([&]() {
            for (auto const& j : cur_queue) {
                j->prepare();
            }
        });

        m_expected_work = 0;
    }

    void commit_thread() {
        assert(!m_running_threads.empty());
        m_running_threads.front().second.join();
        for (auto& j : m_running_threads.front().first) {
            j->commit();
            j.reset();
        }
        m_running_threads.pop_front();
    }

    typedef std::pair<std::vector<job_ptr_type>, std::thread> thread_t;
    thread_t m_next_thread;
    std::deque<thread_t> m_running_threads;

    size_t m_expected_work;
    double m_work_per_thread;
    size_t m_max_threads;
};

template <typename ScoredData>
struct topk_queue {
    explicit topk_queue(uint64_t k) : threshold(0), m_k(k) {}

    bool insert(float score, posting_type docid) {
        if (m_q.size() < m_k) {
            m_q.emplace_back(score, docid);
            std::push_heap(m_q.begin(), m_q.end(),
                           [](auto const& lhs, auto const& rhs) {
                               return lhs.first > rhs.first;
                           });
            threshold = m_q.front().first;
            return true;
        } else {
            if (score > threshold) {
                std::pop_heap(m_q.begin(), m_q.end(),
                              [](auto const& lhs, auto const& rhs) {
                                  return lhs.first > rhs.first;
                              });
                m_q.back() = std::make_pair(score, docid);
                std::push_heap(m_q.begin(), m_q.end(),
                               [](auto const& lhs, auto const& rhs) {
                                   return lhs.first > rhs.first;
                               });
                threshold = m_q.front().first;
                return true;
            }
        }
        return false;
    }

    bool would_enter(float score) const {
        return m_q.size() < m_k or score > threshold;
    }

    void finalize() {
        std::sort_heap(m_q.begin(), m_q.end(),
                       [](auto const& lhs, auto const& rhs) {
                           return lhs.first > rhs.first;
                       });
    }

    ScoredData const& topk() const {
        return m_q;
    }

    void clear() {
        m_q.clear();
    }

    uint64_t size() {
        return m_k;
    }

    float threshold;

private:
    uint64_t m_k;
    ScoredData m_q;
};
}  // namespace pvb
