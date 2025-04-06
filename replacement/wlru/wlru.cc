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
    bankgroup_write_counters(cache->dram->num_channels, std::vector<size_t>(cache->dram->num_bankgroups, 0)),
    bank_open_row_ids(cache->dram->num_channels, std::vector<std::optional<size_t>>(cache->dram->num_bankgroups*cache->dram->num_banks)),
    evict_lookup_sel(ways, SEL_INIT),
    eager_lookup_sel(ways, SEL_INIT),
    test_eviction_pos(static_cast<std::size_t>(sets*ways), -1),
    test_eager_pos(static_cast<std::size_t>(sets*ways), -1),
    set_modulus(sets / NUM_SAMPLED_SETS),
    ilog2_set_modulus(ilog2(set_modulus))
{}

void
wlru::initialize_replacement()
{
    if (opt_bard_use_bitvector)
        fmt::print("BARD: USING BITVECTOR\n");
}

long wlru::find_victim(uint32_t triggering_cpu, uint64_t instr_id, long set, const champsim::cache_block* current_set, champsim::address ip,
                      champsim::address full_addr, access_type type)
{
    auto begin = std::next(std::begin(last_used_cycles), set * NUM_WAYS);
    auto end = std::next(begin, NUM_WAYS);

    const size_t max_evict_lookup = compute_max_lookup(evict_lookup_sel.begin(), evict_lookup_sel.end());
    const size_t max_eager_lookup = compute_max_lookup(eager_lookup_sel.begin(), eager_lookup_sel.end());

    // Compute max lookup:
    if (s_total_evicts % 100'000 == 0)
    {
        std::cout << "max lookup for evict " << s_total_evicts << "\tevict = " << max_evict_lookup << ", eager = " << max_eager_lookup << "\n";
    }

    victim_data victim;
    victim.iter = end;

    victim_data clean_victim;
    victim_data dirty_victim;
    victim_data eager_victim;

    clean_victim.iter = end;
    dirty_victim.iter = end;
    eager_victim.iter = end;
    
    if (is_sampled_set(set))
    {
        // Random selection:
        size_t rand_idx = static_cast<size_t>(std::rand()) % NUM_WAYS;
        
        victim.iter = std::next(begin, rand_idx);
        victim.lru_pos = std::count_if(begin, end,
                                [t=*victim.iter] (auto x) { return t > x; });
        set_victim_data(victim, rand_idx, current_set);
    }
    else
    {
        const size_t max_evict_lookup = compute_max_lookup(evict_lookup_sel.begin(), evict_lookup_sel.end());
        const size_t max_eager_lookup = compute_max_lookup(eager_lookup_sel.begin(), eager_lookup_sel.end());

        size_t ii = 0;
        for (auto it = begin; it != end; it++)
        {
            size_t lru_pos = std::count_if(begin, end,
                                    [t=*it] (auto x) { return t > x; });
            victim_data cand;
            cand.iter = it;
            cand.lru_pos = lru_pos;
            set_victim_data(cand, ii, current_set);

            if (opt_bard_use_bitvector)
            {
                cand.priority = !bank_open_row_ids[cand.channel][cand.bank_idx].has_value();
            }
            else if (opt_dram_page_policy == DRAM_PAGE_POLICY::CLOSE || !opt_bard_use_row_buffer_hits)
            {
                cand.priority = (bankgroup_write_counters[cand.channel][cand.bankgroup] < address_mapper.banks) && !bank_open_row_ids[cand.channel][cand.bank_idx].has_value();
            }
            else
            {
                cand.priority = (bankgroup_write_counters[cand.channel][cand.bankgroup] < address_mapper.banks)
                                        && (!bank_open_row_ids[cand.channel][cand.bank_idx].has_value() || bank_open_row_ids[cand.channel][cand.bank_idx].value() == cand.row);
            }

            if (lru_pos < max_evict_lookup)
            {
                if (cand.dirty)
                {
                    if (dirty_victim.iter == end)
                    {
                        dirty_victim = cand;
                    }
                    else
                    {
                        bool lru_cmp = *it < *dirty_victim.iter;
                        bool evict = ((dirty_victim.priority == cand.priority) && lru_cmp) || ((dirty_victim.priority != cand.priority) && cand.priority);
                        if (evict)
                            dirty_victim = cand;
                    }
                }
                else
                {
                    if (clean_victim.iter == end || (*it < *clean_victim.iter))
                        clean_victim = cand;
                }
            }

            if (lru_pos < max_eager_lookup && cand.dirty && cand.priority)
            {
                if (eager_victim.iter == end || *it < *eager_victim.iter)
                    eager_victim = cand;
            }

            ++ii;
        }

        // Determine final victim at the end:
        if (dirty_victim.iter != end && (clean_victim.iter == end || dirty_victim.priority || *dirty_victim.iter < *clean_victim.iter))
            victim = dirty_victim;
        else
            victim = clean_victim;
    }

    // If this is a sampled set, record the victim's lru position and select the true LRU victim.
    if (is_sampled_set(set))
    {
        auto pos_idx = set*NUM_WAYS + victim.way_id;

        test_eviction_pos[pos_idx] = victim.lru_pos;
        if (victim.dirty)
            test_eager_pos[pos_idx] = victim.lru_pos;

        // Get LRU victim:
        victim.iter = std::min_element(begin, end);
        set_victim_data(victim, std::distance(begin, victim.iter), current_set);

        // Check if `test_eviction_pos` is set for the LRU victim:
        pos_idx = set*NUM_WAYS + victim.way_id;
        if (test_eviction_pos[pos_idx] >= 0)
        {
            auto p = test_eviction_pos[pos_idx];
            if (evict_lookup_sel[p] < SEL_MAX)
                ++evict_lookup_sel[p];
        }
    
        if (test_eager_pos[pos_idx] >= 0)
        {
            auto p = test_eager_pos[pos_idx];
            if (eager_lookup_sel[p] < SEL_MAX)
                ++eager_lookup_sel[p];
        }

        test_eviction_pos[pos_idx] = -1;
        test_eager_pos[pos_idx] = -1;
    }
    else
    {
        if (victim.lru_pos > 0)
            ++s_non_lru_evicts;
        ++s_total_evicts;
    }

    if (!victim.dirty && !is_sampled_set(set))
    {
        if (eager_victim.iter != end)
        {
            // Swap data of eager way and LRU victim:
            auto* _current_set = const_cast<champsim::cache_block*>(current_set);
            _current_set[victim.way_id] = _current_set[eager_victim.way_id];
            _current_set[eager_victim.way_id].dirty = false;

            auto way_id = victim.way_id;
            victim = std::move(eager_victim);
            victim.way_id = way_id;  // Only need to preserve way id which is returned

            ++s_eager_writebacks;
        }
    }

    if (victim.dirty)
    {
        auto& bg_ctrs = bankgroup_write_counters[victim.channel];
        auto& ba_rows = bank_open_row_ids[victim.channel];

        ++bg_ctrs[victim.bankgroup];
        ba_rows[victim.bank_idx] = victim.row;

        bool all_done;
        if (opt_bard_use_bitvector)
            all_done = std::all_of(ba_rows.begin(), ba_rows.end(), [] (auto x) { return x.has_value(); });
        else
            all_done = std::all_of(bg_ctrs.begin(), bg_ctrs.end(), [m=address_mapper.banks] (auto x) { return x >= m; });

        if (all_done)
        {
            std::fill(bg_ctrs.begin(), bg_ctrs.end(), 0);
            for (auto& r : ba_rows)
                r.reset();
        }
    }

    // Find the way whose last use cycle is most distant
    assert(begin <= victim.iter);
    assert(victim.iter < end);
    return victim.way_id;
}

void wlru::replacement_cache_fill(uint32_t triggering_cpu, long set, long way, champsim::address full_addr, champsim::address ip, champsim::address victim_addr,
                                 access_type type)
{
    // Mark the way as being used on the current cycle
    last_used_cycles.at((std::size_t)(set * NUM_WAYS + way)) = cycle++;

    // Reset `test_eviction_pos`
    test_eviction_pos[set*NUM_WAYS + way] = -1;
    test_eager_pos[set*NUM_WAYS + way] = -1;
}

void wlru::update_replacement_state(uint32_t triggering_cpu, long set, long way, champsim::address full_addr, champsim::address ip,
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
                if (evict_lookup_sel[p] > SEL_MIN)
                    --evict_lookup_sel[p];
            }

            p = test_eager_pos[pos_idx];
            if (p >= 0)
            {
                eager_lookup_sel[p] -= (eager_lookup_sel[p] >> 3);
//              if (eager_lookup_sel[p] > SEL_MIN)
//                  --eager_lookup_sel[p];
            }
        }
        else
        {
            last_used_cycles.at((std::size_t)pos_idx) = cycle++;

            int p = test_eviction_pos[pos_idx];
            if (p >= 0)
                evict_lookup_sel[p] -= (evict_lookup_sel[p] >> 2);
        }
    }
}

void
wlru::replacement_final_stats()
{
    fmt::print("WCACHE NON LRU EVICTS: {}\tTOTAL EVICTS: {}\tEAGER WB: {}\n", s_non_lru_evicts, s_total_evicts, s_eager_writebacks);
}

void
wlru::set_victim_data(victim_data& v, size_t idx, const champsim::cache_block* current_set)
{
    v.dirty = current_set[idx].dirty;
    v.way_id = idx;

    auto address = current_set[idx].address;
    v.channel = address_mapper.channel(address);
    v.bankgroup = address_mapper.bankgroup(address);
    v.bank_idx = address_mapper.bank_idx(address);
    v.row = address_mapper.row(address);
}

size_t
wlru::compute_max_lookup(std::vector<int>::const_iterator begin, std::vector<int>::const_iterator end) const
{
    auto it = std::find_if(begin, end, [] (auto x) { return x < SEL_THRESHOLD; });
    return std::distance(begin, it);
}
