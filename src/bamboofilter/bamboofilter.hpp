#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <cmath>
#include <iostream>
#include <vector>

#include "bamboofilter/predefine.h"
#include "bamboofilter/segment.hpp"
#include "common/BOBHash.h"

using std::vector;

class BambooFilter
{
public:
    const uint32_t INIT_TABLE_BITS;
    uint32_t num_table_bits_;

    vector<Segment *> hash_table_;

    uint32_t split_condition_;

    uint32_t next_split_idx_;
    uint32_t num_items_;

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
        return tag & FINGUREPRINT_MASK;
    }

    inline void GenerateIndexTagHash(const char *item, uint32_t &seg_index, uint32_t &bucket_index, uint32_t &tag) const
    {
        const uint32_t hash = BOBHash::run(item, strlen(item), 3);

        bucket_index = BucketIndexHash(hash);
        seg_index = SegIndexHash(hash >> BUCKETS_PER_SEG);
        tag = TagHash(hash >> INIT_TABLE_BITS);

        if (!(tag))
        {
            if (num_table_bits_ > INIT_TABLE_BITS)
            {
                seg_index |= (1 << (INIT_TABLE_BITS - BUCKETS_PER_SEG));
            }
            tag++;
        }

        if (seg_index >= hash_table_.size())
        {
            seg_index = seg_index - (1 << (NUM_SEG_BITS - 1));
        }
    }

public:
    BambooFilter(uint32_t capacity, uint32_t split_condition_param);

    ~BambooFilter();

    bool Insert(const char *key);
    bool Lookup(const char *key) const;
    bool Delete(const char *key);

    void Extend();
    void Compress();
};

BambooFilter::BambooFilter(uint32_t capacity, uint32_t split_condition_param)
    : INIT_TABLE_BITS((uint32_t)ceil(log2((double)(capacity / 4))))
{
    num_table_bits_ = INIT_TABLE_BITS;

    for (int num_segment = 0; num_segment < (1 << NUM_SEG_BITS); num_segment++)
    {
        hash_table_.push_back(new Segment(1 << BUCKETS_PER_SEG));
    }

    split_condition_ = uint32_t(split_condition_param * 4 * (1 << BUCKETS_PER_SEG)) - 1;
    next_split_idx_ = 0;
    num_items_ = 0;
}

BambooFilter::~BambooFilter()
{
    for (uint32_t segment_idx = 0; segment_idx < hash_table_.size(); segment_idx++)
    {
        delete hash_table_[segment_idx];
    }
}

bool BambooFilter::Insert(const char *key)
{
    uint32_t seg_index, bucket_index, tag;

    GenerateIndexTagHash(key, seg_index, bucket_index, tag);

    hash_table_[seg_index]->Insert(bucket_index, tag);

    num_items_++;

    if (!(num_items_ & split_condition_))
    {
        Extend();
    }

    return true;
}

bool BambooFilter::Lookup(const char *key) const
{
    uint32_t seg_index, bucket_index, tag;

    GenerateIndexTagHash(key, seg_index, bucket_index, tag);
    return hash_table_[seg_index]->Lookup(bucket_index, tag);
}

bool BambooFilter::Delete(const char *key)
{
    uint32_t seg_index, bucket_index, tag;
    GenerateIndexTagHash(key, seg_index, bucket_index, tag);

    if (hash_table_[seg_index]->Delete(bucket_index, tag))
    {
        num_items_--;
        if (!(num_items_ & split_condition_))
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

void BambooFilter::Extend()
{
    Segment *src = hash_table_[next_split_idx_];
    Segment *dst = new Segment(*src);
    hash_table_.push_back(dst);

    uint32_t num_seg_bits_ = (uint32_t)ceil(log2((double)hash_table_.size()));
    num_table_bits_ = num_seg_bits_ + BUCKETS_PER_SEG;

    src->EraseEle(true, ACTV_TAG_BIT - 1);
    dst->EraseEle(false, ACTV_TAG_BIT - 1);

    next_split_idx_++;
    if (next_split_idx_ == (1UL << (num_seg_bits_ - 1)))
    {
        next_split_idx_ = 0;
    }
}

void BambooFilter::Compress()
{
    uint32_t num_seg_bits_ = (uint32_t)ceil(log2((double)(hash_table_.size() - 1)));
    num_table_bits_ = num_seg_bits_ + BUCKETS_PER_SEG;
    if (!next_split_idx_)
    {
        next_split_idx_ = (1UL << (num_seg_bits_ - 1));
    }
    next_split_idx_--;

    Segment *src = hash_table_[next_split_idx_];
    Segment *dst = hash_table_.back();
    src->Absorb(dst);
    delete dst;
    hash_table_.pop_back();
}