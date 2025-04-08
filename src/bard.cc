/*
 *  author: Suhas Vittal
 *  date:   7 April 2025
 * */

#include "bard.h"

BARD::mark_recapture_data_type::mark_recapture_data_type(size_t num_pos, long sets, long ways)
    :sel(num_pos, BARD::SEL_INIT),
    test_pos(static_cast<size_t>(sets*ways), -1)
{}

BARD::BARD(size_t num_positions, long sets, long ways, MEMORY_CONTROLLER* dram, bool pos_order)
    :bankgroup_write_counters(dram->num_channels, std::vector<size_t>(dram->num_bankgroups, 0)),
    bank_write_bitvec(dram->num_channels, std::vector<bool>(dram->num_bankgroups*dram->num_banks, false)),
    mr_evict(num_positions, sets, ways),
    mr_eager(num_positions, sets, ways),
    address_mapper(dram->address_mapper),
    NUM_SET(sets),
    NUM_WAY(ways),
    pos_sort_descending(pos_order)
{}

void
BARD::initialize()
{
    set_modulus = NUM_SET / opt_bard_sampled_sets;
    ilog2_set_modulus = ilog2(set_modulus);
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

    const int evp = mr_evict.test_pos[pos_idx];
    const int eap = mr_eager.test_pos[pos_idx];

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
        if (evp >= 0 && mr_evict.sel[evp] < SEL_MAX)
        {
            ++mr_evict.sel[evp];
            mr_evict.sel[evp] = -1;
        }

        if (eap >= 0 && mr_eager.sel[eap] < SEL_MAX)
        {
            ++mr_eager.sel[eap];
            mr_eager.sel[eap] = -1;
        }
    }
}

void
BARD::handle_writeback(champsim::address address)
{
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

long
BARD::find_victim(long initial_victim_way, long set, pos_iterator pos_begin, pos_iterator pos_end, const champsim::cache_block* current_set)
{
    if (is_sampled_set(set)) 
        return initial_victim_way;

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

    if (victim.pos < 0)
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
    if (is_sampled_set(set) || opt_bard_disable_shadow_writeback)
        return -1;

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
        exit(1);

    size_t s = static_cast<size_t>(_set);
    return (s & (set_modulus-1)) == (s >> ilog2_set_modulus);
}

int
BARD::get_max_eviction_pos() const
{
    return compute_max_lookup(mr_evict.sel.begin(), mr_evict.sel.end());
}

int
BARD::get_max_eager_pos() const
{
    return compute_max_lookup(mr_eager.sel.begin(), mr_eager.sel.end());
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

        if (!cand.dirty)
            continue;

        if ((pos_sort_descending && *it < max_lookup) || (!pos_sort_descending && *it >= max_lookup))
        {
            if (opt_bard_use_bitvector)
                cand.priority = !bank_write_bitvec[cand.channel][cand.bank_idx];
            else
                cand.priority = (bankgroup_write_counters[cand.channel][cand.bankgroup] < address_mapper.banks) && !bank_write_bitvec[cand.channel][cand.bank_idx];

            bool cmp = (pos_sort_descending && cand.pos < victim.pos) || (!pos_sort_descending && cand.pos > victim.pos);
            if (cand.priority && (victim.pos < 0 || cmp))
                victim = std::move(cand);
        }
    }

    return victim;
}

int
BARD::compute_max_lookup(std::vector<int>::const_iterator begin, std::vector<int>::const_iterator end) const
{
    if (opt_bard_max_lookup >= 0)
    {
        return opt_bard_max_lookup;
    }
    else
    {
        std::vector<int>::const_iterator it;
        if (pos_sort_descending)
            it = std::find_if(begin, end, [] (auto x) { return x < SEL_THRESHOLD; });
        else
            it = std::find_if(begin, end, [] (auto x) { return x >= SEL_THRESHOLD; });
        return static_cast<int>(std::distance(begin, it));
    }
}

void
cache_set_copy_way_contents_and_clean_source(const champsim::cache_block* _current_set, long from, long to)
{
    auto* current_set = const_cast<champsim::cache_block*>(_current_set);
    current_set[to] = current_set[from];
    current_set[from].dirty = false;
}
