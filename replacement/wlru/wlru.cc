#include "wlru.h"

#include "champsim.h"

#include <algorithm>
#include <cassert>
#include <iostream>

wlru::wlru(CACHE* cache)
    :wlru(cache, cache->NUM_SET, cache->NUM_WAY)
{}

wlru::wlru(CACHE* cache, long sets, long ways)
    :replacement(cache),
    NUM_WAYS(ways),
    last_used_cycles(static_cast<std::size_t>(sets * ways), 0),
    address_mapper(cache->dram->address_mapper),
    dram(cache->dram),
    bank_writeback_done(cache->dram->num_channels, std::vector<bool>(cache->dram->num_bankgroups*cache->dram->num_banks, false)),
    lookup_sel(ways, SEL_INIT),
    test_eviction_pos(static_cast<std::size_t>(sets*ways), -1),
    test_eviction_pos_used(static_cast<std::size_t>(sets*ways), false),
    set_modulus(sets / NUM_SAMPLED_SETS),
    ilog2_set_modulus(ilog2(set_modulus))
{}

void
wlru::initialize_replacement()
{
}

long wlru::find_victim(uint32_t triggering_cpu, uint64_t instr_id, long set, const champsim::cache_block* current_set, champsim::address ip,
                      champsim::address full_addr, access_type type)
{
    auto begin = std::next(std::begin(last_used_cycles), set * NUM_WAYS);
    auto end = std::next(begin, NUM_WAYS);

    // Compute max lookup:
    const size_t max_lookup = is_sampled_set(set) ? NUM_WAYS : compute_max_lookup();

    if (s_total_evicts % 100'000 == 0)
    {
        std::cout << "max lookup for evict " << s_total_evicts << " = " << max_lookup << "\n";
        std::cout << "lookup_sel:";
        for (auto x : lookup_sel)
            std::cout << " " << x;
        std::cout << "\n";
    }

    auto victim = end;
    bool victim_has_writeback_priority = false;
    bool victim_was_dirty = false;
    size_t victim_way;
    size_t victim_channel;
    size_t victim_bank_idx;
    size_t victim_lru_pos;

    size_t ii = 0;
    for (auto it = begin; it != end; it++)
    {
        size_t lru_pos = std::count_if(begin, end,
                                [t=*it] (auto x) { return t > x; });
        if (lru_pos < max_lookup)
        {
            auto address = current_set[ii].address;
            size_t channel = address_mapper.channel(address),
                   b_idx = address_mapper.bank_idx(address);
            bool prio = !bank_writeback_done[channel][b_idx];
            bool dirty = current_set[ii].dirty;

            bool evict;            
            if (victim == end)
            {
                evict = true;
            }
            else
            {
                bool lru_cmp = *it < *victim;
                
                if (dirty && victim_was_dirty)
                    evict = ((prio == victim_has_writeback_priority) && lru_cmp) || ((prio != victim_has_writeback_priority) && prio);
                else if (dirty)
                    evict = prio || lru_cmp;
                else if (victim_was_dirty)
                    evict = !victim_has_writeback_priority && lru_cmp;
                else
                    evict = lru_cmp;
            }

            if (evict)
            {
                victim = it;
                victim_has_writeback_priority = prio;
                victim_was_dirty = dirty;

                victim_way = ii;
                victim_channel = channel;
                victim_bank_idx = b_idx;
                victim_lru_pos = lru_pos;
            }
        }
        ++ii;
    }

    // If this is a sampled set, record the victim's lru position and select the true LRU victim.
    if (is_sampled_set(set))
    {
        test_eviction_pos[set*NUM_WAYS + victim_way] = victim_lru_pos;

        // Get LRU victim:
        victim = std::min_element(begin, end);

        victim_way = std::distance(begin, victim);
        victim_was_dirty = current_set[victim_way].dirty;

        auto address = current_set[victim_way].address;
        victim_channel = address_mapper.channel(address);
        victim_bank_idx = address_mapper.bank_idx(address);

        // Check if `test_eviction_pos` is set for the LRU victim:
        if (test_eviction_pos[set*NUM_WAYS + victim_way] >= 0 && !test_eviction_pos_used[set*NUM_WAYS + victim_way])
        {
            size_t p = test_eviction_pos[set*NUM_WAYS + victim_way];
            if (lookup_sel[p] < SEL_MAX)
                ++lookup_sel[p];
        }
    }
    else
    {
        if (victim_lru_pos > 0)
            ++s_non_lru_evicts;
        ++s_total_evicts;
    }

    if (victim_was_dirty)
    {
        auto& wb = bank_writeback_done[victim_channel];

        wb[victim_bank_idx] = true;

        bool all_done = std::all_of(wb.begin(), wb.end(), [] (bool x) { return x; });
        if (all_done)
            std::fill(wb.begin(), wb.end(), false);
    }

    // Find the way whose last use cycle is most distant
    assert(begin <= victim);
    assert(victim < end);
    return victim_way;
}

void wlru::replacement_cache_fill(uint32_t triggering_cpu, long set, long way, champsim::address full_addr, champsim::address ip, champsim::address victim_addr,
                                 access_type type)
{
    // Mark the way as being used on the current cycle
    last_used_cycles.at((std::size_t)(set * NUM_WAYS + way)) = cycle++;

    // Reset `test_eviction_pos`
    test_eviction_pos[set*NUM_WAYS + way] = -1;
    test_eviction_pos_used[set*NUM_WAYS + way] = false;
}

void wlru::update_replacement_state(uint32_t triggering_cpu, long set, long way, champsim::address full_addr, champsim::address ip,
                                   champsim::address victim_addr, access_type type, uint8_t hit)
{
    // Mark the way as being used on the current cycle
    if (hit)
    {
        if (access_type{type} == access_type::WRITE)
        {
            int p = test_eviction_pos[set*NUM_WAYS + way];
            if (p >= 0)
            {
                if (lookup_sel[p] > SEL_MIN)
                    --lookup_sel[p];
                test_eviction_pos_used[set*NUM_WAYS + way] = true;
            }
        }
        else
        {
            last_used_cycles.at((std::size_t)(set * NUM_WAYS + way)) = cycle++;
            int p = test_eviction_pos[set*NUM_WAYS + way];

            if (p >= 0)
            {
                lookup_sel[p] -= (lookup_sel[p] >> 2);
                test_eviction_pos_used[set*NUM_WAYS + way] = true;
            }
        }
    }
}

void
wlru::replacement_final_stats()
{
    fmt::print("WCACHE NON LRU EVICTS: {}\tTOTAL EVICTS: {}\n", s_non_lru_evicts, s_total_evicts);
}

size_t
wlru::compute_max_lookup() const
{
    /*
    size_t num_drains = 0, num_forced_drains = 0;
    for (size_t i = 0; i < dram->channels.size(); i++)
    {
        num_drains += dram->channels[i]->sim_stats.num_write_drains;
        num_forced_drains += dram->channels[i]->sim_stats.num_forced_write_drains;
    }
    if (num_drains > 256)
    {
        double ratio = ((double)num_forced_drains)/((double)num_drains);
        if (ratio < 0.05)
            return 1;
    }
    */
//  auto it = std::find_if(lookup_sel.rbegin(), lookup_sel.rend(),
//                  [] (auto x) { return x >= SEL_THRESHOLD; });
//  return static_cast<size_t>(NUM_WAYS - std::distance(lookup_sel.rbegin(), it));
    auto it = std::find_if(lookup_sel.begin(), lookup_sel.end(),
                    [] (auto x) { return x < SEL_THRESHOLD; });
    return std::distance(lookup_sel.begin(), it);
}
