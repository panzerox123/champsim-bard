#include "bard_lru.h"

#include <algorithm>
#include <cassert>

bard_lru::bard_lru(CACHE* cache) : bard_lru(cache, cache->NUM_SET, cache->NUM_WAY) {}

bard_lru::bard_lru(CACHE* cache, long sets, long ways)
    :replacement(cache), 
    bard_impl(sets, ways, cache, true),
    NUM_WAY(ways),
    last_used_cycles(static_cast<std::size_t>(sets * ways), 0)
{}

void
bard_lru::initialize_replacement()
{
    bard_impl.initialize();
}

long bard_lru::find_victim(uint32_t triggering_cpu, uint64_t instr_id, long set, const champsim::cache_block* current_set, champsim::address ip,
                      champsim::address full_addr, access_type type)
{
    bard_impl.print_update_msg();

    auto begin = std::next(std::begin(last_used_cycles), set * NUM_WAY);
    auto end = std::next(begin, NUM_WAY);

    // Find the way whose last use cycle is most distant
    auto victim = std::min_element(begin, end);
    assert(begin <= victim);
    assert(victim < end);

    long victim_way = std::distance(begin, victim);

    ++bard_impl.s_total_evicts;

    // Now, use BARD to determine if a different candidate should be selected
    std::vector<int> lru_pos_array(NUM_WAY);
    std::transform(begin, end, lru_pos_array.begin(),
            [begin,end] (auto t)
            {
                return std::count_if(begin, end, [t] (auto x) { return t > x; });
            });

    if (current_set[victim_way].dirty)
    {
        victim_way = bard_impl.find_victim(victim_way, set, lru_pos_array.begin(), lru_pos_array.end(), current_set);
    }
    else
    {
        long shadow_way = bard_impl.find_eager_writeback(set, lru_pos_array.begin(), lru_pos_array.end(), current_set);
        if (shadow_way >= 0)
            cache_set_copy_way_contents_and_clean_source(current_set, shadow_way, victim_way);
    }

    if (current_set[victim_way].dirty)
        bard_impl.handle_writeback(set, current_set[victim_way].address);

    return victim_way;
}

void bard_lru::replacement_cache_fill(uint32_t triggering_cpu, long set, long way, champsim::address full_addr, champsim::address ip, champsim::address victim_addr,
                                 access_type type)
{
  // Mark the way as being used on the current cycle
  last_used_cycles.at((std::size_t)(set * NUM_WAY + way)) = cycle++;
}

void bard_lru::update_replacement_state(uint32_t triggering_cpu, long set, long way, champsim::address full_addr, champsim::address ip,
                                   champsim::address victim_addr, access_type type, uint8_t hit)
{
    // Mark the way as being used on the current cycle
    if (hit && access_type{type} != access_type::WRITE) // Skip this for writeback hits
        last_used_cycles.at((std::size_t)(set * NUM_WAY + way)) = cycle++;
}

void
bard_lru::replacement_final_stats()
{
    return bard_impl.print_stats();
}