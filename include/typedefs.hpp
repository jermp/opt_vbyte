#pragma once

namespace pvb {
    typedef uint32_t posting_type;
    typedef std::pair<float, posting_type> scored_docid_type;
    typedef std::pair<uint64_t, uint64_t> pv_type; // (position, value)
}
