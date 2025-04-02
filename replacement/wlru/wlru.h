#ifndef REPLACEMENT_WLRU_H
#define REPLACEMENT_WLRU_H

#include <vector>

#include "cache.h"
#include "dram_address.h"
#include "modules.h"

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

    std::vector<int> lookup_sel;
    std::vector<int> test_eviction_pos;
    std::vector<bool> test_eviction_pos_used;

    const size_t set_modulus;
    const size_t ilog2_set_modulus;
    /*
     * Stats:
     * */
    uint32_t s_non_lru_evicts =0;
    uint32_t s_total_evicts =0;
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
    size_t compute_max_lookup(void) const;

    inline bool is_sampled_set(size_t idx) const
    {
        return (idx & (set_modulus-1)) == (idx >> ilog2_set_modulus);
    }
};

#endif
