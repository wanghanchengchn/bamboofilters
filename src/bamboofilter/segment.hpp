#include <vector>
#include <iostream>
#include <utility>
#include <string.h>

#include <bitset>

#include "bamboofilter/bitsutil.h"
#include "bamboofilter/predefine.h"

using namespace std;

template <uint32_t bits_per_tag>
class Segment
{
public:
    static const uint32_t kTagsPerBucket = 4;
    static const uint32_t kBytesPerBucket = (bits_per_tag * kTagsPerBucket + 7) >> 3;
    static const uint32_t kTagMask = (1ULL << bits_per_tag) - 1;
    static const uint64_t kBucketMask = (1ULL << (bits_per_tag * kTagsPerBucket)) - 1;

    static const uint32_t kPaddingBuckets =
        ((((kBytesPerBucket + 7) / 8) * 8) - 1) / kBytesPerBucket;

    struct Bucket
    {
        char bits_[kBytesPerBucket];
    } __attribute__((__packed__));

    Bucket *buckets_;
    Segment *overflow;

    uint32_t num_buckets_;

    inline uint32_t IndexHash(uint32_t index) const
    {
        return index & ((1 << BUCKETS_PER_SEG) - 1);
    }

    inline size_t AltIndex(const size_t index, const uint32_t tag) const
    {
        return IndexHash((uint32_t)(index ^ tag));
    }

public:
    Segment(const size_t num_buckets_)
    {
        this->buckets_ = new Bucket[num_buckets_ + kPaddingBuckets];
        this->overflow = NULL;
        this->num_buckets_ = num_buckets_;

        memset(buckets_, 0, kBytesPerBucket * (num_buckets_ + kPaddingBuckets));
    }

    ~Segment()
    {
        for (Segment *p = this; p != NULL; p = p->overflow)
        {
            delete[] p->buckets_;
        }
    }

    // read tag from pos(i,j)
    inline uint32_t ReadTag(const uint32_t i, const uint32_t j) const
    {
        const char *p = buckets_[i].bits_;
        uint32_t tag;

        if (bits_per_tag == 12)
        {
            p += j + (j >> 1);
            tag = *((uint16_t *)p) >> ((j & 1) << 2);
        }
        return tag & kTagMask;
    }

    // write tag to pos(i,j)
    inline void WriteTag(const uint32_t i, const uint32_t j, const uint32_t tag)
    {
        char *p = buckets_[i].bits_;
        uint32_t t = tag & kTagMask;

        if (bits_per_tag == 12)
        {
            p += (j + (j >> 1));
            if ((j & 1) == 0)
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
    }

    inline bool FindTagInBucket(const size_t i, const uint32_t tag) const
    {
        if (bits_per_tag == 12 && kTagsPerBucket == 4)
        {
            const char *p = buckets_[i].bits_;
            uint64_t v = *(uint64_t *)p;
            return hasvalue12(v, tag);
        }
    }

    inline bool FindTagInBuckets(const size_t i1, const size_t i2,
                                 const uint32_t tag) const
    {
        const char *p1 = buckets_[i1].bits_;
        const char *p2 = buckets_[i2].bits_;

        uint64_t v1 = *((uint64_t *)p1);
        uint64_t v2 = *((uint64_t *)p2);

        if (bits_per_tag == 12 && kTagsPerBucket == 4)
        {
            return hasvalue12(v1, tag) || hasvalue12(v2, tag);
        }
    }

    inline bool DeleteTagFromBuckets(const size_t i1, const size_t i2, const uint32_t tag)
    {
        for (size_t j = 0; j < kTagsPerBucket; j++)
        {
            if (ReadTag(i1, j) == tag)
            {
                WriteTag(i1, j, 0);
                return true;
            }
        }
        for (size_t j = 0; j < kTagsPerBucket; j++)
        {
            if (ReadTag(i2, j) == tag)
            {
                WriteTag(i2, j, 0);
                return true;
            }
        }
        if (NULL != overflow)
        {
            return overflow->DeleteTagFromBuckets(i1, i2, tag);
        }
        return false;
    }

    inline bool InsertTagToBucket(const size_t i, const uint32_t tag,
                                  const bool kickout, uint32_t &oldtag)
    {
        for (size_t j = 0; j < kTagsPerBucket; j++)
        {
            if (0 == ReadTag(i, j))
            {
                WriteTag(i, j, tag);
                return true;
            }
        }
        if (kickout)
        {
            size_t r = rand() % kTagsPerBucket;
            oldtag = ReadTag(i, r);
            WriteTag(i, r, tag);
        }
        return false;
    }

    inline bool Insert(const uint32_t bucket_index, const uint32_t tag)
    {
        uint32_t curindex = bucket_index;
        uint32_t curtag = tag;
        uint32_t oldtag;

        for (uint32_t count = 0; count < MAX_CUCKOO_KICK; count++)
        {
            bool kickout = count > 0;
            oldtag = 0;
            if (InsertTagToBucket(curindex, curtag, kickout, oldtag))
            {
                return true;
            }
            if (kickout)
            {
                curtag = oldtag;
            }
            curindex = AltIndex(curindex, curtag);
        }

        if (overflow == NULL)
        {
            overflow = new Segment(num_buckets_);
        }
        return overflow->Insert(curindex, curtag);
    }

    bool Search(const uint32_t first_bucket, const uint32_t second_bucket, const uint32_t tag) const
    {
        if (FindTagInBuckets(first_bucket, second_bucket, tag))
        {
            return true;
        }
        else if (!overflow)
        {
            return false;
        }
        else
        {
            return overflow->Search(first_bucket, second_bucket, tag);
        }
    }

    void Rehash(Segment *seg, uint32_t actv_bit)
    {
        for (uint32_t bucket_index = 0; bucket_index < num_buckets_ + kPaddingBuckets; bucket_index++)
        {
            char *p1 = buckets_[bucket_index].bits_;
            char *p2 = seg->buckets_[bucket_index].bits_;

            uint64_t t = (*(uint64_t *)p1) & kBucketMask;

            ((uint64_t *)p2)[0] &= 0xffff000000000000;
            ((uint64_t *)p2)[0] |= ll_isn(t, actv_bit);

            ((uint64_t *)p1)[0] &= 0xffff000000000000;
            ((uint64_t *)p1)[0] |= ll_isl(t, actv_bit);
        }

        if (overflow)
        {
            if (seg->overflow == NULL)
                seg->overflow = new Segment(num_buckets_);
            overflow->Rehash(seg->overflow, actv_bit);
        }
    }

    void Compress(Segment *segment)
    {
        for (Segment *p_seg = segment; p_seg != NULL; p_seg = p_seg->overflow)
        {
            for (uint32_t i = 0; i < num_buckets_ + kPaddingBuckets; i++)
            {
                for (uint32_t j = 0; j < kTagsPerBucket; j++)
                {
                    uint32_t tag = p_seg->ReadTag(i, j);

                    if (tag == 0)
                        continue;

                    this->Insert(i, tag);
                }
            }
        }
    }
};
