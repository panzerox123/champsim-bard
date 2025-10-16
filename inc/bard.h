/*
 *  author: Suhas Vittal
 *  date:   7 April 2025
 * */

#ifndef BARD_h
#define BARD_h

#include <vector>

#include "cache.h"
#include "dram_address.h"
#include "dram_channel.h"

struct bard_victim_data
{
    bool priority =false;
    bool dirty =false;

    long way_id =-1;

    size_t channel;
    size_t bankgroup;
    size_t bank_idx;
    size_t row;
    
    int pos;
};

class BARD
{
public:
    uint32_t s_non_lru_evicts{0};
    uint32_t s_eager_writebacks{0};
    uint32_t s_total_evicts{0};
    uint32_t s_redundant_writebacks{0};
    uint32_t s_sync_messages{0};

    // set to true if LRU (lower position = earlier timestamp)
    // set to false if RRIP (higher RRPV == evict)
    const bool evict_lower_positions;
private:
    using position_type = int;

    std::vector<std::vector<bool>> bank_write_bitvec;

    CACHE* cache;
    DRAM_ADDRESS_MAPPER address_mapper;

    const long NUM_SET;
    const long NUM_WAY;
public:
    using pos_iterator = std::vector<position_type>::const_iterator;

    BARD(long sets, long ways, CACHE*, bool evict_lower_positions);

    void print_update_msg();

    void initialize(void);
    void print_stats(void);

    void handle_writeback(long set, champsim::address);

    long find_victim(long initial_victim_way, long set, pos_iterator pos_begin, pos_iterator pos_end, const champsim::cache_block*);
    long find_eager_writeback(long set, pos_iterator pos_begin, pos_iterator pos_end, const champsim::cache_block*);
private:
    void set_victim_data(bard_victim_data&, long way, const champsim::cache_block* current_set);
    bard_victim_data select_dirty_line(pos_iterator, pos_iterator, const champsim::cache_block* current_set);

    void check_if_writeback_is_redundant(const bard_victim_data&);
};

void cache_set_copy_way_contents_and_clean_source(const champsim::cache_block*, long from, long to);

/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////

extern DRAM_PAGE_POLICY OPT_DRAM_PAGE_POLICY;

extern int OPT_BARD_MODE;

constexpr int BARD_MODE_DEFAULT{0};
constexpr int BARD_MODE_CLEAN_ONLY{1};
constexpr int BARD_MODE_EVICT_ONLY{2};

/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////

#endif   // BARD_h
