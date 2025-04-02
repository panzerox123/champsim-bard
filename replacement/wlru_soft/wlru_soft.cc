#include "wlru_soft.h"

#include "champsim.h"

#include <algorithm>
#include <cassert>
#include <iostream>

wlru_soft::wlru_soft(CACHE* cache)
    :wlru_soft(cache, cache->NUM_SET, cache->NUM_WAY)
{}

wlru_soft::wlru_soft(CACHE* cache, long sets, long ways)
    :replacement(cache),
    NUM_WAYS(ways),
    last_used_cycles(static_cast<std::size_t>(sets * ways), 0),
    address_mapper(cache->dram->address_mapper),
    dram(cache->dram),
    bankgroup_write_counters(cache->dram->num_channels, std::vector<size_t>(cache->dram->num_bankgroups, 0)),
    bank_open_row_ids(cache->dram->num_channels, std::vector<std::optional<size_t>>(cache->dram->num_bankgroups*cache->dram->num_banks)),
    lookup_sel(ways, SEL_INIT),
    test_eviction_pos(static_cast<std::size_t>(sets*ways), -1),
    test_eviction_pos_used(static_cast<std::size_t>(sets*ways), false),
    set_modulus(sets / NUM_SAMPLED_SETS),
    ilog2_set_modulus(ilog2(set_modulus))
{}

void
wlru_soft::initialize_replacement()
{
}

long wlru_soft::find_victim(uint32_t triggering_cpu, uint64_t instr_id, long set, const champsim::cache_block* current_set, champsim::address ip,
                      champsim::address full_addr, access_type type)
{
    auto begin = std::next(std::begin(last_used_cycles), set * NUM_WAYS);
    auto end = std::next(begin, NUM_WAYS);

    // Compute max lookup:

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
    size_t victim_bankgroup;
    size_t victim_bank_idx;
    size_t victim_row_id;
    size_t victim_lru_pos;
    
    if (is_sampled_set(set))
    {
        // Random selection:
        size_t rand_idx = static_cast<size_t>(std::rand()) % NUM_WAYS;
        victim = std::next(begin, rand_idx);

        victim_was_dirty = current_set[rand_idx].dirty;
        victim_way = rand_idx;

        auto address = current_set[rand_idx].address;
        victim_channel = address_mapper.channel(address);
        victim_bankgroup = address_mapper.bankgroup(address);
        victim_bank_idx = address_mapper.bank_idx(address);
        victim_row_id = address_mapper.row(address);

        victim_lru_pos = std::count_if(begin, end,
                                [t=*victim] (auto x) { return t > x; });
    }
    else
    {
        const size_t max_lookup = compute_max_lookup();

        size_t ii = 0;
        for (auto it = begin; it != end; it++)
        {
            size_t lru_pos = std::count_if(begin, end,
                                    [t=*it] (auto x) { return t > x; });
            if (lru_pos < max_lookup)
            {
                auto address = current_set[ii].address;
                size_t channel = address_mapper.channel(address),
                       bg = address_mapper.bankgroup(address),
                       b_idx = address_mapper.bank_idx(address),
                       row_id = address_mapper.row(address);

                bool dirty = current_set[ii].dirty;
                bool prio = (bankgroup_write_counters[channel][bg] < address_mapper.banks)
                            && (!bank_open_row_ids[channel][b_idx].has_value() || bank_open_row_ids[channel][b_idx].value() == row_id);

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
                        evict = prio || (lru_cmp && !victim_has_writeback_priority);
                    else if (victim_was_dirty)
                        evict = !victim_has_writeback_priority && (lru_cmp && !prio);
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
                    victim_bankgroup = bg;
                    victim_bank_idx = b_idx;
                    victim_row_id = row_id;
                    victim_lru_pos = lru_pos;
                }
            }
            ++ii;
        }
    }

    // If this is a sampled set, record the victim's lru position and select the true LRU victim.
    if (is_sampled_set(set))
    {
        size_t pos_idx = set*NUM_WAYS + victim_way;
        // If the evition pos of the simulated victim is already set, consume it:
        if (test_eviction_pos[pos_idx] >= 0 && !test_eviction_pos_used[pos_idx])
        {
            auto p = test_eviction_pos[pos_idx];
            if (lookup_sel[p] < SEL_MAX)
                ++lookup_sel[p];
        }
        test_eviction_pos[pos_idx] = victim_lru_pos;

        // Get LRU victim:
        victim = std::min_element(begin, end);

        victim_way = std::distance(begin, victim);
        victim_was_dirty = current_set[victim_way].dirty;

        auto address = current_set[victim_way].address;
        victim_channel = address_mapper.channel(address);
        victim_bankgroup = address_mapper.bankgroup(address);
        victim_bank_idx = address_mapper.bank_idx(address);
        victim_row_id = address_mapper.row(address);

        // Check if `test_eviction_pos` is set for the LRU victim:
        size_t pos_idx = set*NUM_WAYS + victim_way;
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
        auto& wbg = bankgroup_write_counters[victim_channel];
        auto& wba = bank_open_row_ids[victim_channel];

        ++wbg[victim_bankgroup];
        wba[victim_bank_idx] = victim_row_id;

        bool all_done = std::all_of(wbg.begin(), wbg.end(), [m=address_mapper.banks] (auto x) { return x >= m; });
        if (all_done)
        {
            std::fill(wbg.begin(), wbg.end(), 0);
            for (auto& r : wba)
                r.reset();
        }
    }

    // Find the way whose last use cycle is most distant
    assert(begin <= victim);
    assert(victim < end);
    return victim_way;
}

void wlru_soft::replacement_cache_fill(uint32_t triggering_cpu, long set, long way, champsim::address full_addr, champsim::address ip, champsim::address victim_addr,
                                 access_type type)
{
    // Mark the way as being used on the current cycle
    last_used_cycles.at((std::size_t)(set * NUM_WAYS + way)) = cycle++;

    // Reset `test_eviction_pos`
    test_eviction_pos[set*NUM_WAYS + way] = -1;
    test_eviction_pos_used[set*NUM_WAYS + way] = false;
}

void wlru_soft::update_replacement_state(uint32_t triggering_cpu, long set, long way, champsim::address full_addr, champsim::address ip,
                                   champsim::address victim_addr, access_type type, uint8_t hit)
{
    // Mark the way as being used on the current cycle
    size_t pos_idx = set*NUM_WAYS + way;
    if (hit)
    {
        if (access_type{type} == access_type::WRITE)
        {
            int p = test_eviction_pos[pos_idx];
            if (p >= 0)
            {
                if (lookup_sel[p] > SEL_MIN)
                    --lookup_sel[p];
                test_eviction_pos_used[pos_idx] = true;
            }
        }
        else
        {
            last_used_cycles.at((std::size_t)pos_idx) = cycle++;

            int p = test_eviction_pos[pos_idx];
            if (p >= 0)
            {
                lookup_sel[p] -= (lookup_sel[p] >> 3);
                test_eviction_pos_used[pos_idx] = true;
            }
        }
    }
}

void
wlru_soft::replacement_final_stats()
{
    fmt::print("WCACHE NON LRU EVICTS: {}\tTOTAL EVICTS: {}\n", s_non_lru_evicts, s_total_evicts);
}

size_t
wlru_soft::compute_max_lookup() const
{
    auto it = std::find_if(lookup_sel.begin(), lookup_sel.end(),
                    [] (auto x) { return x < SEL_THRESHOLD; });
    return std::distance(lookup_sel.begin(), it);
}
