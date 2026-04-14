#ifndef REPLACEMENT_BARD_PLUS_SRRIP_H
#define REPLACEMENT_BARD_PLUS_SRRIP_H

#include <cstdint>
#include <random>
#include <vector>

#include "bard.h"
#include "cache.h"
#include "modules.h"

class bard_plus_srrip : public champsim::modules::replacement
{
public:
    constexpr static int RRPV_MIN = 0;
    constexpr static int RRPV_MAX = 3;

    // Probability of bypassing BARD and using vanilla SRRIP
    static constexpr double SRRIP_BYPASS_PROB = 0.05;
private:
    BARD bard_impl;

    const long NUM_SET;
    const long NUM_WAY;
    std::vector<int> rrpv;

    // RNG for binomial bypass decision
    std::mt19937 rng{42};
    std::bernoulli_distribution bypass_dist{SRRIP_BYPASS_PROB};

    int next_rand_rrpv = RRPV_MIN;
public:
    explicit bard_plus_srrip(CACHE* cache);
    bard_plus_srrip(CACHE* cache, long sets_, long ways_);

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

    void replacement_cache_fill(uint32_t triggering_cpu, long set, long way, champsim::address full_addr, champsim::address ip, champsim::address victim_addr,
                              access_type type)
    {
        update_replacement_state(triggering_cpu, set, way, full_addr, ip, victim_addr, type, 0);
    }
};

#endif
