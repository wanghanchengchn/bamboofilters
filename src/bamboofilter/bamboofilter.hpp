#include "bamboofilter/bitsutil.h"
#include "common/BOBHash.h"
#include "bamboofilter/segment.hpp"
#include "bamboofilter/predefine.h"

using namespace std;

template <uint32_t bits_per_item>
class BambooFilter
{
public:
    uint32_t num_items_;
    uint32_t next_point_;
    uint32_t num_table_bits_;

    uint32_t INIT_TABLE_BITS;

    vector<Segment<bits_per_item> *> segments_;

    inline uint32_t BucketIndexHash(uint32_t index) const
    {
        return index & ((1 << BUCKETS_PER_SEG) - 1);
    }

    inline uint32_t SegIndexHash(uint32_t index) const
    {
        return index & ((1 << NUM_SEG_BITS) - 1);
    }

    inline uint32_t TagHash(uint32_t tag) const
    {
        return tag & ((1 << bits_per_item) - 1);
    }

    inline size_t AltIndex(const size_t index, const uint32_t tag) const
    {
        return BucketIndexHash((uint32_t)(index ^ tag));
    }

    inline void GenerateIndexTagHash(const char *item, uint32_t *bucket_index, uint32_t *seg_index, uint32_t *tag) const
    {
        const uint32_t hash = BOBHash::run(item, strlen(item), 3);

        *bucket_index = BucketIndexHash(hash);
        *seg_index = SegIndexHash(hash >> BUCKETS_PER_SEG);
        *tag = TagHash(hash >> INIT_TABLE_BITS);

        if (!(*tag))
        {
            if (num_table_bits_ > INIT_TABLE_BITS)
            {
                *seg_index |= (1 << (INIT_TABLE_BITS - BUCKETS_PER_SEG));
            }
            *tag = *tag + 1;
        }

        if (*seg_index >= segments_.size())
        {
            *seg_index = *seg_index - (1 << (NUM_SEG_BITS - 1));
        }
    }

public:
    BambooFilter(uint32_t exp_items_)
    {
        num_items_ = 0;
        next_point_ = 0;

        INIT_TABLE_BITS = ceil(log2((double)(exp_items_ / 4)));

        num_table_bits_ = INIT_TABLE_BITS;

        for (int num_seg = 0; num_seg < (1 << NUM_SEG_BITS); num_seg++)
        {
            segments_.push_back(new Segment<bits_per_item>(1 << BUCKETS_PER_SEG));
        }
    }

    ~BambooFilter()
    {
        for (uint32_t idx = 0; idx < segments_.size(); idx++)
        {
            delete segments_[idx];
        }
    }

    void Insert(const char *key);
    bool Lookup(const char *key) const;
    bool Delete(const char *key);

    void Extend();
    void Compress();
};

template <uint32_t bits_per_item>
void BambooFilter<bits_per_item>::Insert(const char *key)
{
    uint32_t bucket_index, seg_index, tag;

    GenerateIndexTagHash(key, &bucket_index, &seg_index, &tag);

    segments_[seg_index]->Insert(bucket_index, tag);

    num_items_++;

    if (!(num_items_ & SPLIT_CONDITION))
    {
        Extend();
    }
}

template <uint32_t bits_per_item>
bool BambooFilter<bits_per_item>::Lookup(const char *key) const
{
    uint32_t seg_index, first_bucket, second_bucket, tag;

    GenerateIndexTagHash(key, &first_bucket, &seg_index, &tag);

    second_bucket = AltIndex(first_bucket, tag);

    return segments_[seg_index]->Search(first_bucket, second_bucket, tag);
}

template <uint32_t bits_per_item>
bool BambooFilter<bits_per_item>::Delete(const char *key)
{
    uint32_t seg_index, first_bucket, second_bucket, tag;

    GenerateIndexTagHash(key, &first_bucket, &seg_index, &tag);

    second_bucket = AltIndex(first_bucket, tag);

    if (segments_[seg_index]->DeleteTagFromBuckets(first_bucket, second_bucket, tag))
    {
        num_items_--;

        if (!(num_items_ & SPLIT_CONDITION))
        {
            Compress();
        }
        return true;
    }
    else
    {
        return false;
    }
}

template <uint32_t bits_per_item>
void BambooFilter<bits_per_item>::Extend()
{
    segments_.push_back(new Segment<bits_per_item>(1 << BUCKETS_PER_SEG));

    uint32_t num_seg_bits_ = ceil(log2((double)segments_.size()));

    num_table_bits_ = num_seg_bits_ + BUCKETS_PER_SEG;

    segments_[next_point_]->Rehash(segments_[(next_point_ | (1 << (num_seg_bits_ - 1)))], (ACTV_TAG_BIT - 1));

    next_point_++;

    if (next_point_ == (1UL << (num_seg_bits_ - 1)))
    {
        next_point_ = 0;
    }
}

template <uint32_t bits_per_item>
void BambooFilter<bits_per_item>::Compress()
{
    uint32_t num_seg_bits_ = ceil(log2((double)(segments_.size() - 1)));

    num_table_bits_ = num_seg_bits_ + BUCKETS_PER_SEG;

    if (!next_point_)
    {
        next_point_ = (1UL << (num_seg_bits_ - 1));
    }

    next_point_--;

    segments_[next_point_]->Compress(segments_[segments_.size() - 1]);

    delete segments_[segments_.size() - 1];
    segments_.pop_back();
}
