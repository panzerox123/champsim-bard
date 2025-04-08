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

    std::vector<std::vector<size_t>> bankgroup_write_counters;
    std::vector<std::vector<bool>> bank_write_bitvec;
    mark_recapture_data_type mr_evict;
    mark_recapture_data_type mr_eager;

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

    BARD(size_t num_positions, long sets, long ways, MEMORY_CONTROLLER*, bool pos_order);

    void initialize(void);
    void print_stats(void);

    void handle_mark(long set, long rand_way, position_type pos, bool dirty);
    void handle_recapture(long set, long way, RecaptureType);
    void handle_writeback(champsim::address);

    long find_victim(long initial_victim_way, long set, pos_iterator pos_begin, pos_iterator pos_end, const champsim::cache_block*);
    long find_eager_writeback(long set, pos_iterator pos_begin, pos_iterator pos_end, const champsim::cache_block*);

    bool is_sampled_set(long set) const;

    int get_max_eviction_pos(void) const;
    int get_max_eager_pos(void) const;
private:
    void set_victim_data(bard_victim_data&, long way, const champsim::cache_block* current_set);

    bard_victim_data select_dirty_line(pos_iterator pos_begin, pos_iterator pos_end, const long max_lookup, const champsim::cache_block*);
    int compute_max_lookup(std::vector<int>::const_iterator, std::vector<int>::const_iterator) const;
};

void cache_set_copy_way_contents_and_clean_source(const champsim::cache_block*, long from, long to);

/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////

extern DRAM_PAGE_POLICY opt_dram_page_policy;
extern bool             opt_bard_use_row_buffer_hits;
extern bool             opt_bard_use_bitvector;
extern bool             opt_bard_disable_shadow_writeback;

extern int              opt_bard_max_lookup;
extern int              opt_bard_sampled_sets;

/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////

#endif   // BARD_h
