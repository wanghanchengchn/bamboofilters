#include <immintrin.h>
#include <stdint.h>
#include <stdlib.h>

#include <iostream>

#include "bamboofilter/bitsutil.h"
#include "bamboofilter/predefine.h"

using namespace std;

class Segment
{
private:
    // const
    static const uint32_t kTagsPerBucket = 4;
    static const uint32_t kBytesPerBucket = (BITS_PER_TAG * kTagsPerBucket + 7) >> 3;

    static const uint32_t kTagMask = (1ULL << BITS_PER_TAG) - 1;
    static const uint64_t kBucketMask = (1ULL << (BITS_PER_TAG * kTagsPerBucket)) - 1;

    static const uint32_t bucket_size = (BITS_PER_TAG * kTagsPerBucket + 7) / 8; // kBytesPerBucket
    static const uint32_t safe_pad = sizeof(uint64_t) - bucket_size;
    static const uint32_t safe_pad_simd = 4; // 4B for avx2

private:
    char *temp;
    const uint32_t chain_num;
    uint32_t chain_capacity;
    uint32_t total_size;
    uint32_t insert_cur;
    char *data_base;
    uint32_t ANS_MASK;

    static uint32_t IndexHash(uint32_t index)
    {
        return index & ((1 << BUCKETS_PER_SEG) - 1);
    }
    static uint32_t AltIndex(size_t index, uint32_t tag)
    {
        return IndexHash((uint32_t)(index ^ tag));
    }

    static void WriteTag(char *p, uint32_t idx, uint32_t tag)
    {
        uint32_t t = tag & kTagMask;
        p += (idx + (idx >> 1));
        if ((idx & 1) == 0)
        {
            ((uint16_t *)p)[0] &= 0xf000;
            ((uint16_t *)p)[0] |= t;
        }
        else
        {
            ((uint16_t *)p)[0] &= 0x000f;
            ((uint16_t *)p)[0] |= (t << 4);
        }
    }

    static uint32_t ReadTag(const char *p, uint32_t idx)
    {
        uint32_t tag;
        p += idx + (idx >> 1);
        tag = *((uint16_t *)p) >> ((idx & 1) << 2);
        return tag & kTagMask;
    }

    static bool LookupTag(const char *p, uint32_t tag)
    {
        uint64_t v = *((uint64_t *)p);
        return hasvalue12(v, tag);
    }

    static bool RemoveOnCondition(const char *p, uint32_t idx, uint32_t old_tag)
    {
        p += idx + (idx >> 1);
        uint32_t tag = (*((uint16_t *)p) >> ((idx & 1) << 2)) & kTagMask;
        if (old_tag != tag & kTagMask)
        {
            return false;
        }

        if ((idx & 1) == 0)
        {
            ((uint16_t *)p)[0] &= 0xf000;
        }
        else
        {
            ((uint16_t *)p)[0] &= 0x000f;
        }
        return true;
    }

    static bool DeleteTag(char *p, uint32_t tag)
    {
        for (size_t tag_idx = 0; tag_idx < kTagsPerBucket; tag_idx++)
        {
            if (RemoveOnCondition(p, tag_idx, tag))
            {
                return true;
            }
        }
        return false;
    }

    static void doErase(char *p, bool is_src, uint32_t actv_bit)
    {
        uint64_t v = (*(uint64_t *)p) & kBucketMask;

        ((uint64_t *)p)[0] &= 0xffff000000000000;
        ((uint64_t *)p)[0] |= is_src ? ll_isl(v, actv_bit) : ll_isn(v, actv_bit);
    }

    static __m256i unpack12to16(const char *p)
    {
        __m256i v = _mm256_loadu_si256((const __m256i *)(p - 4));

        const __m256i bytegrouping =
            _mm256_setr_epi8(4, 5, 5, 6, 7, 8, 8, 9, 10, 11, 11, 12, 13, 14, 14, 15,
                             0, 1, 1, 2, 3, 4, 4, 5, 6, 7, 7, 8, 9, 10, 10, 11);
        v = _mm256_shuffle_epi8(v, bytegrouping);

        __m256i hi = _mm256_srli_epi16(v, 4);
        __m256i lo = _mm256_and_si256(v, _mm256_set1_epi32(0x00000FFF));

        return _mm256_blend_epi16(lo, hi, 0b10101010);
    }

public:
    Segment(const uint32_t chain_num)
        : chain_num(chain_num),
          chain_capacity(1),
          insert_cur(0),
          ANS_MASK(~(0xFFFFFFFF << 2 * (2 * chain_capacity * kTagsPerBucket % 16)))
    {
        total_size = chain_num * chain_capacity * bucket_size + safe_pad;
        data_base = new char[total_size];
        memset(data_base, 0, (chain_num * chain_capacity * bucket_size));
        temp = new char[safe_pad_simd + (2 * chain_capacity * bucket_size + 23) / 24 * 24 + safe_pad_simd];
    }

    Segment(const Segment &s)
        : chain_num(s.chain_num),
          chain_capacity(s.chain_capacity),
          total_size(s.total_size),
          insert_cur(0),
          ANS_MASK(s.ANS_MASK)
    {
        data_base = new char[total_size];
        temp = new char[safe_pad_simd + (2 * chain_capacity * bucket_size + 23) / 24 * 24 + safe_pad_simd];
        memcpy(data_base, s.data_base, total_size);
    }

    ~Segment()
    {
        delete[] data_base;
        delete[] temp;
    };

    bool Insert(uint32_t chain_idx, uint32_t curtag)
    {
        char *bucket_p;
        for (uint32_t count = 0; count < MAX_CUCKOO_KICK; count++)
        {
            bucket_p = data_base + (chain_idx * chain_capacity + insert_cur) * bucket_size;
            bool kickout = count > 0;

            for (size_t tag_idx = 0; tag_idx < kTagsPerBucket; tag_idx++)
            {
                if (0 == ReadTag(bucket_p, tag_idx))
                {
                    WriteTag(bucket_p, tag_idx, curtag);
                    return true;
                }
            }

            if (kickout)
            {
                size_t tag_idx = rand() % kTagsPerBucket;
                uint32_t oldtag = ReadTag(bucket_p, tag_idx);
                WriteTag(bucket_p, tag_idx, curtag);
                curtag = oldtag;
            }
            chain_idx = AltIndex(chain_idx, curtag);
            bucket_p = data_base + (chain_idx * chain_capacity + insert_cur) * bucket_size;
        }

        insert_cur++;
        if (insert_cur >= chain_capacity)
        {
            char *old_data_base = data_base;
            uint32_t old_chain_len = chain_capacity * bucket_size;
            chain_capacity++;
            uint32_t new_chain_len = chain_capacity * bucket_size;
            ANS_MASK = ~(0xFFFFFFFF << 2 * (2 * chain_capacity * kTagsPerBucket % 16));
            delete[] temp;
            temp = new char[safe_pad_simd + (2 * chain_capacity * bucket_size + 23) / 24 * 24 + safe_pad_simd];

            total_size = chain_num * chain_capacity * bucket_size + safe_pad;
            data_base = new char[total_size];
            memset(data_base, 0, total_size);
            for (int i = 0; i < chain_num; i++)
            {
                memcpy(data_base + i * new_chain_len, old_data_base + i * old_chain_len, old_chain_len);
            }
            delete[] old_data_base;
        }
        return Insert(chain_idx, curtag);
    }

    bool Lookup(uint32_t chain_idx, uint16_t tag) const
    {
        memcpy(temp + safe_pad_simd,
               data_base + chain_idx * chain_capacity * bucket_size,
               chain_capacity * bucket_size);
        memcpy(temp + safe_pad_simd + chain_capacity * bucket_size,
               data_base + AltIndex(chain_idx, tag) * chain_capacity * bucket_size,
               chain_capacity * bucket_size);
        char *p = temp + safe_pad_simd;
        char *end = p + 2 * chain_capacity * bucket_size;

        __m256i _true_tag = _mm256_set1_epi16(tag);
        while (p + 24 <= end)
        {
            __m256i _16_tags = unpack12to16(p);

            __m256i _ans = _mm256_cmpeq_epi16(_16_tags, _true_tag);
            if (_mm256_movemask_epi8(_ans))
            {
                return true;
            }
            p += 24;
        }
        __m256i _16_tags = unpack12to16(p);

        __m256i _ans = _mm256_cmpeq_epi16(_16_tags, _true_tag);
        if (ANS_MASK & _mm256_movemask_epi8(_ans))
        {
            return true;
        }
        return false;
    }

    bool Delete(uint32_t chain_idx, uint32_t tag)
    {
        uint32_t chain_idx2 = AltIndex(chain_idx, tag);
        for (int i = 0; i < chain_capacity; i++)
        {
            char *p = data_base + (chain_idx * chain_capacity + i) * bucket_size;
            if (DeleteTag(p, tag))
            {
                return true;
            }
        }
        for (int i = 0; i < chain_capacity; i++)
        {
            char *p = data_base + (chain_idx2 * chain_capacity + i) * bucket_size;
            if (DeleteTag(p, tag))
            {
                return true;
            }
        }
        return false;
    }

    void EraseEle(bool is_src, uint32_t actv_bit)
    {
        for (int i = 0; i < chain_num * chain_capacity; i++)
        {
            char *p = data_base + i * bucket_size;
            doErase(p, is_src, actv_bit);
        }
        insert_cur = 0;
    }

    void Absorb(const Segment *segment)
    {
        char *p1 = data_base;
        uint32_t len1 = (chain_capacity * bucket_size);
        char *p2 = segment->data_base;
        uint32_t len2 = (segment->chain_capacity * segment->bucket_size);

        chain_capacity += segment->chain_capacity;
        insert_cur = 0;
        ANS_MASK = ~(0xFFFFFFFF << 2 * (2 * chain_capacity * kTagsPerBucket % 16));

        total_size = chain_num * chain_capacity * bucket_size + safe_pad;
        data_base = new char[total_size];

        for (int i = 0; i < chain_num; i++)
        {
            memcpy(data_base + i * (len1 + len2), p1 + i * len1, len1);
            memcpy(data_base + i * (len1 + len2) + len1, p2 + i * len2, len2);
        }
        delete[] p1;
        delete[] temp;
        temp = new char[safe_pad_simd + (2 * chain_capacity * bucket_size + 23) / 24 * 24 + safe_pad_simd];
    }
};
