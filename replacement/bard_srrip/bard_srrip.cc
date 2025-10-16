#include "bard_srrip.h"

#include <algorithm>
#include <cassert>
#include <unordered_map>

#include "cache.h"

bard_srrip::bard_srrip(CACHE* cache) : bard_srrip(cache, cache->NUM_SET, cache->NUM_WAY) {}

bard_srrip::bard_srrip(CACHE* cache, long sets_, long ways_)
    :replacement(cache),
    bard_impl(sets_, ways_, cache, false),
    NUM_SET(sets_),
    NUM_WAY(ways_),
    rrpv(sets_*ways_, RRPV_MAX)
{}

// find replacement victim
long
bard_srrip::find_victim(uint32_t triggering_cpu, uint64_t instr_id, long set, const champsim::cache_block* current_set, champsim::address ip,
                        champsim::address full_addr, access_type type)
{
    bard_impl.print_update_msg();

    auto begin = std::next(rrpv.begin(), set*NUM_WAY);
    auto end = std::next(begin, NUM_WAY);

    auto v_it = std::max_element(begin, end);
    long victim_way = std::distance(begin, v_it);

    // Check for an alternate candidate:
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
        bard_impl.handle_writeback(set, current_set[victim_way].address);

    ++bard_impl.s_total_evicts;

    return victim_way;
}

// called on every cache hit and cache fill
void
bard_srrip::update_replacement_state(uint32_t triggering_cpu, long set, long way, champsim::address full_addr, champsim::address ip,
                                     champsim::address victim_addr, access_type type, uint8_t hit)
{
    if (way == NUM_WAY)
        return;

    // Update bard:
    if (hit && access_type{type} == access_type::WRITE)
        return;

    int initial_rrpv = RRPV_MAX-1;
    rrpv[set*NUM_WAY + way] = hit ? 0 : initial_rrpv;
}
