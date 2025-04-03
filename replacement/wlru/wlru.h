#ifndef REPLACEMENT_WLRU_H
#define REPLACEMENT_WLRU_H

#include <vector>

#include "cache.h"
#include "dram_address.h"
#include "dram_channel.h"
#include "modules.h"

struct victim_data
{
    std::vector<uint64_t>::iterator iter;

    bool priority =false;
    bool dirty =false;

    long way_id;

    size_t channel;
    size_t bankgroup;
    size_t bank_idx;
    size_t row;
    
    size_t lru_pos;
};

void set_victim_data(victim_data&, size_t idx, const champsim::cache_block* set);

class wlru : public champsim::modules::replacement
{
private:
    constexpr static size_t SEL_WIDTH = 8;
    constexpr static int SEL_MIN = 0;
    constexpr static int SEL_MAX = (1<<SEL_WIDTH)-1;
    constexpr static int SEL_INIT = SEL_MAX/2+1;
    constexpr static int SEL_THRESHOLD = SEL_INIT;

    constexpr static size_t NUM_SAMPLED_SETS = 64;

    long NUM_WAYS;
    std::vector<uint64_t> last_used_cycles;
    uint64_t cycle = 0;

    DRAM_ADDRESS_MAPPER address_mapper;
    MEMORY_CONTROLLER* dram;

    std::vector<std::vector<size_t>> bankgroup_write_counters;
    std::vector<std::vector<std::optional<size_t>>> bank_open_row_ids;

    std::vector<int> evict_lookup_sel;
    std::vector<int> eager_lookup_sel;

    std::vector<int> test_eviction_pos;
    std::vector<int> test_eager_pos;

    const size_t set_modulus;
    const size_t ilog2_set_modulus;
    /*
     * Stats:
     * */
    uint32_t s_non_lru_evicts =0;
    uint32_t s_total_evicts =0;
    uint32_t s_eager_writebacks =0;
public:
    explicit wlru(CACHE* cache);
    wlru(CACHE* cache, long sets, long ways);

    void initialize_replacement();

    long find_victim(uint32_t triggering_cpu, uint64_t instr_id, long set, const champsim::cache_block* current_set, champsim::address ip,
                   champsim::address full_addr, access_type type);
    void replacement_cache_fill(uint32_t triggering_cpu, long set, long way, champsim::address full_addr, champsim::address ip, champsim::address victim_addr,
                              access_type type);
    void update_replacement_state(uint32_t triggering_cpu, long set, long way, champsim::address full_addr, champsim::address ip, champsim::address victim_addr,
                                access_type type, uint8_t hit);

    void replacement_final_stats();
private:
    void set_victim_data(victim_data&, size_t idx, const champsim::cache_block* current_set);

    long find_eager_candidate(long set, const champsim::cache_block* current_set);
    size_t compute_max_lookup(std::vector<int>::const_iterator, std::vector<int>::const_iterator) const;

    inline bool is_sampled_set(size_t idx) const
    {
        return (idx & (set_modulus-1)) == (idx >> ilog2_set_modulus);
    }
};

extern DRAM_PAGE_POLICY opt_dram_page_policy;

#endif
