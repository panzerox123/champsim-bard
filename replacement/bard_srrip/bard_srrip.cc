#include "bard_srrip.h"

#include <algorithm>
#include <cassert>
#include <unordered_map>

#include "cache.h"

bard_srrip::bard_srrip(CACHE* cache) : bard_srrip(cache, cache->NUM_SET, cache->NUM_WAY) {}

bard_srrip::bard_srrip(CACHE* cache, long sets_, long ways_)
    :replacement(cache),
    bard_impl(4, sets_, ways_, cache->dram, false),
    NUM_SET(sets_),
    NUM_WAY(ways_),
    rrpv(sets_*ways_, RRPV_MAX)
{}

// find replacement victim
long
bard_srrip::find_victim(uint32_t triggering_cpu, uint64_t instr_id, long set, const champsim::cache_block* current_set, champsim::address ip,
                        champsim::address full_addr, access_type type)
{
    const int max_lookup = bard_impl.get_max_eviction_pos();

    auto begin = std::next(rrpv.begin(), set*NUM_WAY);
    auto end = std::next(begin, NUM_WAY);

    if (bard_impl.is_sampled_set(set))
    {
        long rand_way = std::rand();
        auto rand_it = std::next(begin, rand_way);
        int r = *rand_it;

        bard_impl.handle_mark(set, rand_way, r, current_set[rand_way].dirty);
    }

    auto v_it = std::max_element(begin, end);
    if (max_lookup < 4 && *v_it < max_lookup)
    {
        // Compute delta from max lookup:
        int d = max_lookup - *v_it;

        // Increment all rrpvs by this much:
        std::for_each(begin, end, [d] (auto& x) { x += d; });
    }

    long victim_way = std::distance(begin, v_it);

    // Check for an alternate candidate:
    if (bard_impl.is_sampled_set(set))
    {
        bard_impl.handle_recapture(set, victim_way, BARD::RecaptureType::EVICT);
    }
    else
    {
        if (current_set[victim_way].dirty)
        {
            victim_way = bard_impl.find_victim(victim_way, set, begin, end, current_set);
        }
        else
        {
            long shadow_way = bard_impl.find_eager_writeback(set, begin, end, current_set);
            if (shadow_way >= 0)
                cache_set_copy_way_contents_and_clean_source(current_set, shadow_way, victim_way);
        }
    }

    // Increment RRPVs one final time
    v_it = std::next(begin, victim_way);
    int d = RRPV_MAX - *v_it;
    std::for_each(begin, end, 
            [d] (auto& x) 
            { 
                x += d; 
                if (x > RRPV_MAX)
                    x = RRPV_MAX;
            });

    if (current_set[victim_way].dirty)
        bard_impl.handle_writeback(current_set[victim_way].address);

    return victim_way;
}

// called on every cache hit and cache fill
void
bard_srrip::update_replacement_state(uint32_t triggering_cpu, long set, long way, champsim::address full_addr, champsim::address ip,
                                     champsim::address victim_addr, access_type type, uint8_t hit)
{
    int initial_rrpv = hit ? 0 : bard_impl.get_max_eviction_pos()-1;
    if (initial_rrpv < RRPV_MIN)  // Can occur when max lookup = 0
        initial_rrpv = RRPV_MIN;

    rrpv[set*NUM_WAY + way] = hit ? 0 : initial_rrpv;

    // Update bard:
    if (hit)
    {
        if (access_type{type} == access_type::WRITE)
            bard_impl.handle_recapture(set, way, BARD::RecaptureType::WRITE_HIT);
        else
            bard_impl.handle_recapture(set, way, BARD::RecaptureType::LOAD_HIT);
    }
}
