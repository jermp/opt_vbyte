#pragma once

namespace pvb {

    struct global_parameters {
        global_parameters()
            : ef_log_sampling0(9)
            , ef_log_sampling1(8)
            , rb_log_rank1_sampling(9)
            , rb_log_sampling1(8)
            , log_partition_size(7)

            , blocks(3, 0)
            , dense_avg_gap(0.0)
            , sparse_avg_gap(0.0)
        {}

        template <typename Visitor>
        void map(Visitor& visit)
        {
            visit
                (ef_log_sampling0, "ef_log_sampling0")
                (ef_log_sampling1, "ef_log_sampling1")
                (rb_log_rank1_sampling, "rb_log_rank1_sampling")
                (rb_log_sampling1, "rb_log_sampling1")
                (log_partition_size, "log_partition_size")
                ;
        }

        uint8_t ef_log_sampling0;
        uint8_t ef_log_sampling1;
        uint8_t rb_log_rank1_sampling;
        uint8_t rb_log_sampling1;
        uint8_t log_partition_size;

        /*
            blocks[0] = # accessed blocks of type 0
            blocks[1] = # accessed blocks of type 1
            blocks[2] = # accessed blocks of type 2
        */
        std::vector<uint64_t> blocks;

        // avg. gap of dense and sparse blocks
        double dense_avg_gap;
        double sparse_avg_gap;
    };

}
