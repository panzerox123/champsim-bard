/*
 *  author: Suhas Vittal
 *  date:   7 April 2025
 * */

#include "bard.h"

BARD::mark_recapture_data_type::mark_recapture_data_type(size_t num_pos, long sets, long ways)
    :sel(num_pos, BARD::SEL_INIT),
    test_pos(static_cast<size_t>(sets*ways), -1)
{}

BARD::utility_monitor::utility_monitor(size_t num_pos)
    :hits(num_pos, 0)
{}

void
BARD::utility_monitor::update_max_lookup(bool pos_sort_descending, int shift)
{
    int miss_count = misses;
    max_lookup = pos_sort_descending ? hits.size() : 0;

    // Discount LRU position as it will never be forcefully evicted
    const int i_start = pos_sort_descending ? 1 : hits.size() - 2;
    const int i_end = pos_sort_descending ? hits.size() : -1;

    int i = i_start;
    while (i != i_end)
    {
        int new_miss_count = miss_count + hits[i];
        if (new_miss_count <= misses + (misses>>shift))
        {
            miss_count = new_miss_count;
        }
        else
        {
            max_lookup = i;
            break;
        }

        if (pos_sort_descending)
            ++i;
        else
            --i;
    }
}

void
BARD::utility_monitor::divide_counters()
{
    for (auto& c : hits)
        c >>= 1;
    misses >>= 1;
}

BARD::BARD(size_t num_positions, long sets, long ways, CACHE* cache, bool pos_order)
    :bankgroup_write_counters(cache->dram->num_channels, std::vector<size_t>(cache->dram->num_bankgroups, 0)),
    bank_write_bitvec(cache->dram->num_channels, std::vector<bool>(cache->dram->num_bankgroups*cache->dram->num_banks, false)),
    mr_evict(num_positions, sets, ways),
    mr_eager(num_positions, sets, ways),
    load_umon(num_positions),
    write_umon(num_positions),
    cache(cache),
    address_mapper(cache->dram->address_mapper),
    NUM_SET(sets),
    NUM_WAY(ways),
    pos_sort_descending(pos_order)
{}

void
BARD::print_update_msg()
{
    if (s_total_evicts % 100'000 == 0)
    {
        fmt::print("num evicts: {}, p: {}, s: {} \t max proactive lookup: {}, max shadow lookup: {}\n", s_total_evicts, s_non_lru_evicts, s_eager_writebacks, get_max_eviction_pos(), get_max_eager_pos());

        if (opt_bard_use_utility_counters)
        {
            fmt::print("current cycle = {}, last update cycle = {}, delta = {}\n",
                    cache->current_cycle(), last_update_cycle, cache->current_cycle() - last_update_cycle);

            fmt::print("\tload umon |\tmisses = {}, hits=", load_umon.misses);
            for (auto& c : load_umon.hits)
                fmt::print(" {}", c);
            fmt::print("\n\twrite umon |\tmisses = {}, hits=", write_umon.misses);
            for (auto& c : write_umon.hits)
                fmt::print(" {}", c);
            fmt::print("\n");
        }
        else
        {
            fmt::print("\tevict counters:");
            for (int x : mr_evict.sel)
                fmt::print(" {}", x);

            fmt::print("\n\teager counters:");
            for (int x : mr_eager.sel)
                fmt::print(" {}", x);

            fmt::print("\n");
        }
    }
}

void
BARD::initialize()
{
    set_modulus = NUM_SET / opt_bard_sampled_sets;
    ilog2_set_modulus = ilog2(set_modulus);
    fmt::print("initializing BARD -- num sampled sets: {}, modulus = {}\n", opt_bard_sampled_sets, set_modulus);
}

void
BARD::print_stats()
{
    fmt::print("BARD NON LRU EVICTS: {}\tTOTAL EVICTS: {}\tEAGER WB: {}\n", s_non_lru_evicts, s_total_evicts, s_eager_writebacks);
}

void
BARD::handle_mark(long set, long rand_way, position_type pos, bool dirty)
{
    if (!is_sampled_set(set))
        return;

    size_t pos_idx = static_cast<size_t>(set*NUM_WAY + rand_way);

    mr_evict.test_pos[pos_idx] = pos;
    if (dirty)
        mr_eager.test_pos[pos_idx] = pos;
}

void
BARD::handle_recapture(long set, long way, RecaptureType r)
{
    if (!is_sampled_set(set))
        return;

    size_t pos_idx = static_cast<size_t>(set*NUM_WAY + way);

    int& evp = mr_evict.test_pos[pos_idx];
    int& eap = mr_eager.test_pos[pos_idx];

    if (r == RecaptureType::LOAD_HIT)
    {
        if (evp >= 0)
            mr_evict.sel[evp] -= (mr_evict.sel[evp] >> 2);
    }
    else if (r == RecaptureType::WRITE_HIT)
    {
        if (evp >= 0 && mr_evict.sel[evp] > SEL_MIN)
            --mr_evict.sel[evp];

        if (eap >= 0)
            mr_eager.sel[eap] -= (mr_eager.sel[eap] >> 3);
    }
    else
    {
        if (evp >= 0)
        {
            if (mr_evict.sel[evp] < SEL_MAX)
                ++mr_evict.sel[evp];
            evp = -1;
        }

        if (eap >= 0)
        {
            if (mr_eager.sel[eap] < SEL_MAX)
                ++mr_eager.sel[eap];
            eap = -1;
        }
    }
}

void
BARD::handle_writeback(long set, champsim::address address)
{
    if (is_sampled_set(set))
    {
        ++write_umon.misses;
    }

    auto channel = address_mapper.channel(address);
    auto bankgroup = address_mapper.bankgroup(address);
    auto bank_idx = address_mapper.bank_idx(address);

    auto& bg_ctrs = bankgroup_write_counters[channel];
    auto& ba_bits = bank_write_bitvec[channel];

    ++bg_ctrs[bankgroup];
    ba_bits[bank_idx] = true;

    bool all_done;
    if (opt_bard_use_bitvector)
        all_done = std::all_of(ba_bits.begin(), ba_bits.end(), [] (bool x) { return x; });
    else
        all_done = std::all_of(bg_ctrs.begin(), bg_ctrs.end(), [m=address_mapper.banks] (auto x) { return x >= m; });

    if (all_done)
    {
        std::fill(bg_ctrs.begin(), bg_ctrs.end(), 0);
        std::fill(ba_bits.begin(), ba_bits.end(), false);
    }
}

void
BARD::handle_hit_miss(long set, long way, position_type pos, bool is_write, bool is_miss)
{
    if (!is_sampled_set(set))
        return;

    if (is_write)
    {
        if (!is_miss && cache->block[set*NUM_WAY+way].dirty)
            ++write_umon.hits[pos];
    }
    else
    {
        if (is_miss)
            ++load_umon.misses;
        else if (cache->block[set*NUM_WAY+way].dirty)
            ++load_umon.hits[pos];
    }
}

long
BARD::find_victim(long initial_victim_way, long set, pos_iterator pos_begin, pos_iterator pos_end, const champsim::cache_block* current_set)
{
    if (is_sampled_set(set) || opt_bard_only_shadow_writeback) 
        return initial_victim_way;

    update_utility_monitors();

    const long max_lookup = get_max_eviction_pos();

    // If the replacement policy has already selected a victim, check if it is ok:
    bard_victim_data victim;
    if (initial_victim_way >= 0)
    {
        set_victim_data(victim, initial_victim_way, current_set);

        // Do not need to do anything if the victim is not dirty
        if (!victim.dirty)
            return initial_victim_way;

        // If the line is dirty and it improves BLP, then we can also exit early:
        if (victim.dirty)
        {
            set_victim_data(victim, initial_victim_way, current_set);

            bool ok;
            if (opt_bard_use_bitvector)
                ok = !bank_write_bitvec[victim.channel][victim.bank_idx];
            else
                ok = (bankgroup_write_counters[victim.channel][victim.bankgroup] < address_mapper.banks) && !bank_write_bitvec[victim.channel][victim.bank_idx];

            if (ok)
                return initial_victim_way;
        }
    }

    // Otherwise, try to find an entry that improves BLP:
    victim = select_dirty_line(pos_begin, pos_end, max_lookup, current_set);

    if (victim.way_id < 0)
    {
        return initial_victim_way;
    }
    else
    {
        ++s_non_lru_evicts;
        return victim.way_id;
    }
}

long
BARD::find_eager_writeback(long set, pos_iterator pos_begin, pos_iterator pos_end, const champsim::cache_block* current_set)
{
    if (is_sampled_set(set) || opt_bard_only_proactive_writeback)
        return -1;

    update_utility_monitors();

    const long max_lookup = get_max_eager_pos();
    bard_victim_data victim = select_dirty_line(pos_begin, pos_end, max_lookup, current_set);

    if (victim.way_id >= 0)
        ++s_eager_writebacks;
    return victim.way_id;
}

bool
BARD::is_sampled_set(long _set) const
{
    if (set_modulus == 0)
    {
        fmt::print("exiting because set modulus is 0\n");
        exit(1);
    }

    size_t s = static_cast<size_t>(_set);
    return (s & (set_modulus-1)) == (s >> ilog2_set_modulus);
}

int
BARD::get_max_eviction_pos()
{
    if (opt_bard_max_lookup >= 0)
    {
        return opt_bard_max_lookup;
    }
    else if (opt_bard_use_utility_counters)
    {
        load_umon.update_max_lookup(pos_sort_descending, 3);
        return load_umon.max_lookup;
    }
    else
    {
        return compute_max_lookup(mr_evict.sel);
    }
}

int
BARD::get_max_eager_pos()
{
    if (opt_bard_max_lookup >= 0)
    {
        return opt_bard_max_lookup;
    }
    else if (opt_bard_use_utility_counters)
    {
        write_umon.update_max_lookup(pos_sort_descending, 2);
        return write_umon.max_lookup;
    }
    else
    {
        return compute_max_lookup(mr_eager.sel);
    }
}

void
BARD::set_victim_data(bard_victim_data& v, long way, const champsim::cache_block* current_set)
{
    v.dirty = current_set[way].dirty;
    v.way_id = way;

    auto address = current_set[way].address;
    v.channel = address_mapper.channel(address);
    v.bankgroup = address_mapper.bankgroup(address);
    v.bank_idx = address_mapper.bank_idx(address);
    v.row = address_mapper.row(address);
}

bard_victim_data
BARD::select_dirty_line(pos_iterator pos_begin, pos_iterator pos_end, const long max_lookup, const champsim::cache_block* current_set)
{
    bard_victim_data victim{};

    long way = 0;
    for (auto it = pos_begin; it != pos_end; it++)
    {
        bard_victim_data cand;
        cand.pos = *it;
        set_victim_data(cand, way, current_set);
        ++way;

        if (!cand.dirty)
            continue;

        if ((pos_sort_descending && *it < max_lookup) || (!pos_sort_descending && *it >= max_lookup))
        {
            if (opt_bard_use_bitvector)
                cand.priority = !bank_write_bitvec[cand.channel][cand.bank_idx];
            else
                cand.priority = (bankgroup_write_counters[cand.channel][cand.bankgroup] < address_mapper.banks) && !bank_write_bitvec[cand.channel][cand.bank_idx];

            bool cmp = (pos_sort_descending && cand.pos < victim.pos) || (!pos_sort_descending && cand.pos > victim.pos);
            if (cand.priority && (victim.way_id < 0 || cmp))
                victim = std::move(cand);
        }
    }

    return victim;
}

int
BARD::compute_max_lookup(const std::vector<int>& sel) const
{
    if (pos_sort_descending)
    {
        auto it = std::find_if(sel.begin(), sel.end(), [] (auto x) { return x < SEL_THRESHOLD; });
        return std::max(static_cast<int>(std::distance(sel.begin(), it)), 1);
    }
    else
    {
        auto it = std::find_if(sel.rbegin(), sel.rend(), [] (auto x) { return x < SEL_THRESHOLD; });
        return std::min(static_cast<int>(sel.size() - std::distance(sel.rbegin(), it)), static_cast<int>(sel.size())-1);
    }
}

void
BARD::update_utility_monitors()
{
    if (cache->current_cycle() - last_update_cycle >= 5'000'000)
    {
        load_umon.divide_counters();
        write_umon.divide_counters();
        last_update_cycle = cache->current_cycle();
    }
}

void
cache_set_copy_way_contents_and_clean_source(const champsim::cache_block* _current_set, long from, long to)
{
    auto* current_set = const_cast<champsim::cache_block*>(_current_set);
    current_set[to] = current_set[from];
    current_set[from].dirty = false;
}
