#pragma once

#include "FastPFor/headers/optpfor.h"
#include "FastPFor/headers/variablebyte.h"
#include "streamvbyte/include/streamvbyte.h"
#include "MaskedVByte/include/varintencode.h"
#include "MaskedVByte/include/varintdecode.h"

#include "global_parameters.hpp"
#include "succinct/bit_vector.hpp"

#include "VarIntG8IU.h"
#include "varintgb.h"

#include "succinct/util.hpp"
#include "interpolative_coding.hpp"
#include "util.hpp"
#include "typedefs.hpp"

namespace ds2i {

    // workaround: VariableByte::decodeArray needs the buffer size, while we
    // only know the number of values. It also pads to 32 bits. We need to
    // rewrite
    class TightVariableByte {
    public:
        template<uint32_t i>
        static uint8_t extract7bits(const uint32_t val) {
            return static_cast<uint8_t>((val >> (7 * i)) & ((1U << 7) - 1));
        }

        template<uint32_t i>
        static uint8_t extract7bitsmaskless(const uint32_t val) {
            return static_cast<uint8_t>((val >> (7 * i)));
        }

        static void encode(const uint32_t *in, const size_t length,
                           uint8_t *out, size_t& nvalue)
        {
            uint8_t * bout = out;
            for (size_t k = 0; k < length; ++k) {
                const uint32_t val(in[k]);
                /**
                 * Code below could be shorter. Whether it could be faster
                 * depends on your compiler and machine.
                 */
                if (val < (1U << 7)) {
                    *bout = static_cast<uint8_t>(val | (1U << 7));
                    ++bout;
                } else if (val < (1U << 14)) {
                    *bout = extract7bits<0> (val);
                    ++bout;
                    *bout = extract7bitsmaskless<1> (val) | (1U << 7);
                    ++bout;
                } else if (val < (1U << 21)) {
                    *bout = extract7bits<0> (val);
                    ++bout;
                    *bout = extract7bits<1> (val);
                    ++bout;
                    *bout = extract7bitsmaskless<2> (val) | (1U << 7);
                    ++bout;
                } else if (val < (1U << 28)) {
                    *bout = extract7bits<0> (val);
                    ++bout;
                    *bout = extract7bits<1> (val);
                    ++bout;
                    *bout = extract7bits<2> (val);
                    ++bout;
                    *bout = extract7bitsmaskless<3> (val) | (1U << 7);
                    ++bout;
                } else {
                    *bout = extract7bits<0> (val);
                    ++bout;
                    *bout = extract7bits<1> (val);
                    ++bout;
                    *bout = extract7bits<2> (val);
                    ++bout;
                    *bout = extract7bits<3> (val);
                    ++bout;
                    *bout = extract7bitsmaskless<4> (val) | (1U << 7);
                    ++bout;
                }
            }
            nvalue = bout - out;
        }

        static void encode_single(uint32_t val, std::vector<uint8_t>& out)
        {
            uint8_t buf[5];
            size_t nvalue;
            encode(&val, 1, buf, nvalue);
            out.insert(out.end(), buf, buf + nvalue);
        }

        static uint8_t const* decode(const uint8_t *in, uint32_t *out, size_t n)
        {
            const uint8_t * inbyte = in;
            for (size_t i = 0; i < n; ++i) {
                unsigned int shift = 0;
                for (uint32_t v = 0; ; shift += 7) {
                    uint8_t c = *inbyte++;
                    v += ((c & 127) << shift);
                    if ((c & 128)) {
                        *out++ = v;
                        break;
                    }
                }
            }
            return inbyte;
        }
    };

    struct interpolative_block {
        static const uint64_t block_size = 128;

        static void encode(uint32_t const* in, uint32_t sum_of_values,
                           size_t n, std::vector<uint8_t>& out)
        {
            assert(n <= block_size);
            std::vector<uint32_t> inbuf(block_size);
            std::vector<uint32_t> outbuf;
            inbuf[0] = *in;
            for (size_t i = 1; i < n; ++i) {
                inbuf[i] = inbuf[i - 1] + in[i];
            }

            if (sum_of_values == uint32_t(-1)) {
                sum_of_values = inbuf[n - 1];
                TightVariableByte::encode_single(sum_of_values, out);
            }

            bit_writer bw(outbuf);
            bw.write_interpolative(inbuf.data(), n - 1, 0, sum_of_values);
            uint8_t const* bufptr = (uint8_t const*)outbuf.data();
            out.insert(out.end(), bufptr,
                       bufptr + succinct::util::ceil_div(bw.size(), 8));
        }

        static uint8_t const* DS2I_NOINLINE decode(uint8_t const* in, uint32_t* out,
                                                 uint32_t sum_of_values, size_t n)
        {
            assert(n <= block_size);
            uint8_t const* inbuf = in;
            if (sum_of_values == uint32_t(-1)) {
                inbuf = TightVariableByte::decode(inbuf, &sum_of_values, 1);
            }

            out[n - 1] = sum_of_values;
            size_t read_interpolative = 0;
            if (n > 1) {
                bit_reader br((uint32_t const*)inbuf);
                br.read_interpolative(out, n - 1, 0, sum_of_values);
                for (size_t i = n - 1; i > 0; --i) {
                    out[i] -= out[i - 1];
                }
                read_interpolative = succinct::util::ceil_div(br.position(), 8);
            }

            return inbuf + read_interpolative;
        }
    };

    struct varintg8iu_block {
        static const uint64_t block_size = 128;

        struct codec_type : VarIntG8IU {

            // rewritten version of decodeBlock optimized for when the output
            // size is known rather than the input
            // the buffers pointed by src and dst must be respectively at least
            // 9 and 8 elements large
            uint32_t decodeBlock(uint8_t const*& src, uint32_t* dst) const
            {
                uint8_t desc = *src;
                src += 1;
                const __m128i data = _mm_lddqu_si128 (reinterpret_cast<__m128i const*>(src));
                src += 8;
                const __m128i result = _mm_shuffle_epi8 (data, vecmask[desc][0]);
                _mm_storeu_si128(reinterpret_cast<__m128i*> (dst), result);
                int readSize = maskOutputSize[desc];

                if ( readSize > 4 ) {
                    const __m128i result2 = _mm_shuffle_epi8 (data, vecmask[desc][1]);//__builtin_ia32_pshufb128(data, shf2);
                    _mm_storeu_si128(reinterpret_cast<__m128i *> (dst + 4), result2);//__builtin_ia32_storedqu(dst + (16), result2);
                }

                return readSize;
            }
        };

        static void encode(uint32_t const* in, uint32_t sum_of_values,
                           size_t n, std::vector<uint8_t>& out)
        {
            codec_type varint_codec;
            std::vector<uint8_t> buf(2 * 4 * block_size);
            assert(n <= block_size);

            if (n < 8) {
                interpolative_block::encode(in, sum_of_values, n, out);
                return;
            }

            size_t out_len = buf.size();

            const uint32_t * src = in;
            unsigned char* dst = buf.data();
            size_t srclen = n * 4;
            size_t dstlen = out_len;
            out_len = 0;
            while (srclen > 0 && dstlen >= 9) {
                auto out = varint_codec.encodeBlock(src, srclen, dst, dstlen);
                out_len += out;
            }
            assert(srclen == 0);
            out.insert(out.end(), buf.data(), buf.data() + out_len);
        }

        // we only allow varint to be inlined (others have DS2I_NOILINE)
        static uint8_t const* decode(uint8_t const* in, uint32_t* out,
                                     uint32_t sum_of_values, size_t n)
        {
            static codec_type varint_codec; // decodeBlock is thread-safe
            assert(n <= block_size);

            if (DS2I_UNLIKELY(n < 8)) {
                return interpolative_block::decode(in, out, sum_of_values, n);
            }

            size_t out_len = 0;
            uint8_t const* src = in;
            uint32_t* dst = out;
            while (out_len <= (n - 8)) {
                auto out = varint_codec.decodeBlock(src, dst + out_len);
                out_len += out;
            }

            // decodeBlock can overshoot, so we decode the last blocks in a
            // local buffer
            while (out_len < n) {
                uint32_t buf[8];
                size_t read = varint_codec.decodeBlock(src, buf);
                size_t needed = std::min(read, n - out_len);
                memcpy(dst + out_len, buf, needed * 4);
                out_len += needed;
            }
            assert(out_len == n);
            return src;
        }
    };

    struct streamvbyte_block {
        static const uint64_t block_size = 128;
        static void encode(uint32_t const *in,
                           uint32_t sum_of_values,
                           size_t n,
                           std::vector<uint8_t> &out) {

            assert(n <= block_size);
            if (n < block_size) {
                interpolative_block::encode(in, sum_of_values, n, out);
                return;
            }
            uint32_t *src = const_cast<uint32_t *>(in);
            std::vector<uint8_t> buf(streamvbyte_max_compressedbytes(block_size));
            size_t out_len = streamvbyte_encode(src, n, buf.data());
            out.insert(out.end(), buf.data(), buf.data() + out_len);
        }
        static uint8_t const *decode(uint8_t const *in,
                                     uint32_t *out,
                                     uint32_t sum_of_values,
                                     size_t n) {
            assert(n <= block_size);
            if (DS2I_UNLIKELY(n < block_size)) {
                return interpolative_block::decode(in, out, sum_of_values, n);
            }
            auto read = streamvbyte_decode(in, out, n);
            return in + read;
        }
    };

    struct maskedvbyte_block {

        static const uint64_t block_size = 128;
        static const int type = 0;

        static inline uint64_t posting_cost(posting_type x, uint64_t base) {
            if (x == 0 or x - base == 0) {
                return 8;
            }

            assert(x >= base);
            return 8 * succinct::util::ceil_div(
                ceil_log2(x - base + 1), // delta gap
                7
            );
        }

        template<typename Iterator>
        static DS2I_FLATTEN_FUNC uint64_t
        bitsize(Iterator begin, global_parameters const& params,
                uint64_t universe, uint64_t n, uint64_t base = 0)
        {
            (void) params;
            (void) universe;
            uint64_t cost = 0;
            auto it = begin;
            for (uint64_t i = 0; i < n; ++i, ++it) {
                cost += posting_cost(*it, base);
                base = *it;
            }
            return cost;
        }

        template<typename Iterator>
        static void write(succinct::bit_vector_builder& bvb,
                          Iterator begin, uint64_t base,
                          uint64_t universe, uint64_t n,
                          global_parameters const& params)
        {
            (void) params;
            std::vector<uint32_t> docids;
            docids.reserve(n);
            uint32_t last_doc(-1);
            for (size_t i = 0; i < n; ++i) {
                uint32_t doc = *(begin + i) - base;
                docids.push_back(doc - last_doc); // delta gap
                last_doc = doc;
            }
            std::vector<uint8_t> out;
            encode(docids.data(), universe, n, out);
            for (uint8_t v: out) {
                bvb.append_bits(v, 8);
            }
        }

        static void encode(uint32_t const *in,
                           uint32_t sum_of_values,
                           size_t n,
                           std::vector<uint8_t>& out)
        {
            (void) sum_of_values;
            size_t size = n;
            uint32_t *src = const_cast<uint32_t *>(in);
            std::vector<uint8_t> buf(2 * size * sizeof(uint32_t));
            size_t out_len = vbyte_encode(src, size, buf.data());
            out.insert(out.end(), buf.data(), buf.data() + out_len);
        }

        static uint8_t const* decode(uint8_t const *in,
                                     uint32_t *out,
                                     uint32_t sum_of_values,
                                     size_t n)
        {
            (void) sum_of_values;
            auto read = masked_vbyte_decode(in, out, n);
            return in + read;
        }
    };

    struct varintgb_block {
        static const uint64_t block_size = 128;

        static void
        encode(uint32_t const *in, uint32_t sum_of_values, size_t n, std::vector<uint8_t> &out) {
            VarIntGB<false> varintgb_codec;
            assert(n <= block_size);
            if (n < block_size) {
                interpolative_block::encode(in, sum_of_values, n, out);
                return;
            }
            std::vector<uint8_t> buf(2 * n * sizeof(uint32_t));
            size_t out_len = varintgb_codec.encodeArray(in, n, buf.data());
            out.insert(out.end(), buf.data(), buf.data() + out_len);
        }

        static uint8_t const *decode(uint8_t const *in,
                                     uint32_t *out,
                                     uint32_t sum_of_values,
                                     size_t n) {
            VarIntGB<false> varintgb_codec;
            assert(n <= block_size);
             if (DS2I_UNLIKELY(n < block_size)) {
                return interpolative_block::decode(in, out, sum_of_values, n);
            }
            auto read = varintgb_codec.decodeArray(in, n, out);
            return read + in;
        }
    };
}
