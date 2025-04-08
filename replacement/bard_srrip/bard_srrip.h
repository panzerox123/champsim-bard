#ifndef REPLACEMENT_BARD_SRRIP_H
#define REPLACEMENT_BARD_SRRIP_H

#include <cstdint>
#include <vector>

#include "bard.h"
#include "cache.h"
#include "modules.h"

class bard_srrip : public champsim::modules::replacement
{
public:
    constexpr static int RRPV_MIN = 0;
    constexpr static int RRPV_MAX = 3;
private:
    BARD bard_impl;

    const long NUM_SET;
    const long NUM_WAY;
    std::vector<int> rrpv;
public:
    explicit bard_srrip(CACHE* cache);
    bard_srrip(CACHE* cache, long sets_, long ways_);

    long find_victim(uint32_t triggering_cpu, uint64_t instr_id, long set, const champsim::cache_block* current_set, champsim::address ip,
                   champsim::address full_addr, access_type type);
    void update_replacement_state(uint32_t triggering_cpu, long set, long way, champsim::address full_addr, champsim::address ip, champsim::address victim_addr,
                                access_type type, uint8_t hit);

    void initialize_replacement()
    {
        bard_impl.initialize();
    }

    // use this function to print out your own stats at the end of simulation
    void replacement_final_stats()
    {
        bard_impl.print_stats();
    }
};

#endif
