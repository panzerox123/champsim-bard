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
    constexpr static size_t SEL_WIDTH = 8;
    constexpr static int    SEL_MIN = 0;
    constexpr static int    SEL_MAX = (1<<SEL_WIDTH)-1;
    constexpr static int    SEL_INIT = SEL_MAX/2 + 1;
    constexpr static int    SEL_THRESHOLD = SEL_INIT;

    enum class RecaptureType { LOAD_HIT, WRITE_HIT, EVICT };

    uint32_t s_non_lru_evicts =0;
    uint32_t s_eager_writebacks =0;
    uint32_t s_total_evicts =0;
private:
    using position_type = int;

    struct mark_recapture_data_type
    {
        std::vector<int> sel;
        std::vector<position_type> test_pos;

        mark_recapture_data_type(size_t num_positions, long sets, long ways);
    };

    struct utility_monitor
    {
        std::vector<int> hits;
        int misses =0;

        int max_lookup = 0;

        utility_monitor(size_t num_positions);

        void update_max_lookup(bool pos_sort_descending, int shift);
        void divide_counters(void);
    };

    std::vector<std::vector<size_t>> bankgroup_write_counters;
    std::vector<std::vector<bool>> bank_write_bitvec;
    mark_recapture_data_type mr_evict;
    mark_recapture_data_type mr_eager;

    utility_monitor load_umon;
    utility_monitor write_umon;
    uint64_t last_update_cycle =0;

    CACHE* cache;
    DRAM_ADDRESS_MAPPER address_mapper;

    const long NUM_SET;
    const long NUM_WAY;

    /*
     * Initialized in `initialize`
     * */
    size_t set_modulus =0;
    size_t ilog2_set_modulus =0;

    const bool pos_sort_descending;
public:
    using pos_iterator = std::vector<position_type>::const_iterator;

    BARD(size_t num_positions, long sets, long ways, CACHE*, bool pos_order);

    void print_update_msg(void);

    void initialize(void);
    void print_stats(void);

    void handle_mark(long set, long rand_way, position_type, bool dirty);
    void handle_recapture(long set, long way, RecaptureType);
    void handle_writeback(long set, champsim::address);

    void handle_hit_miss(long set, long way, position_type, bool is_write, bool is_miss);

    long find_victim(long initial_victim_way, long set, pos_iterator pos_begin, pos_iterator pos_end, const champsim::cache_block*);
    long find_eager_writeback(long set, pos_iterator pos_begin, pos_iterator pos_end, const champsim::cache_block*);

    bool is_sampled_set(long set) const;

    int get_max_eviction_pos(void);
    int get_max_eager_pos(void);
private:
    void set_victim_data(bard_victim_data&, long way, const champsim::cache_block* current_set);

    bard_victim_data select_dirty_line(pos_iterator pos_begin, pos_iterator pos_end, const long max_lookup, const champsim::cache_block*);
    int compute_max_lookup(const std::vector<int>&) const;

    void update_utility_monitors(void);
};

void cache_set_copy_way_contents_and_clean_source(const champsim::cache_block*, long from, long to);

/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////

extern DRAM_PAGE_POLICY OPT_DRAM_PAGE_POLICY;
extern bool             OPT_BARD_USE_ROW_BUFFER_HITS;
extern bool             OPT_BARD_USE_BITVECTOR;

extern bool             OPT_BARD_ONLY_PROACTIVE_WRITEBACK;
extern bool             OPT_BARD_ONLY_SHADOW_WRITEBACK;

extern int              OPT_BARD_MAX_LOOKUP;
extern int              OPT_BARD_SAMPLED_SETS;

extern bool             OPT_BARD_USE_UTILITY_COUNTERS;

/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////

#endif   // BARD_h
