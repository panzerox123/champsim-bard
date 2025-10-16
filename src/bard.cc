/*
 *  author: Suhas Vittal
 *  date:   7 April 2025
 * */

#include "bard.h"

BARD::BARD(long sets, long ways, CACHE* _cache, bool _evict_lower_positions)
    :bank_write_bitvec(cache->dram->num_channels, std::vector<bool>(cache->dram->num_bankgroups*cache->dram->num_banks, false)),
    evict_lower_positions(_evict_lower_positions),
    cache(_cache),
    address_mapper(_cache->dram->address_mapper),
    NUM_SET(sets),
    NUM_WAY(ways)
{}

void
BARD::print_update_msg()
{
}

void
BARD::initialize()
{
}

void
BARD::print_stats()
{
    fmt::print("BARD_TOTAL_EVICTS : {}\n", s_total_evicts);
    fmt::print("BARD_NON_LRU_EVICTS : {}\n", s_non_lru_evicts);
    fmt::print("BARD_EAGER_WRITEBACKS : {}\n", s_eager_writebacks);
    fmt::print("BARD_REDUNDANT_WRITEBACKS : {\n", s_redundant_writebacks);
}

void
BARD::handle_writeback(long set, champsim::address address)
{
    auto channel_id = address_mapper.channel(address);
    auto bank_idx = address_mapper.bank_idx(address);

    auto& ba_bits = bank_write_bitvec[channel_id];

    // only need to synchronize BLP-Vectors if bit is not already set
    if (!ba_bits[bank_idx])
        s_sync_messages++;

    ba_bits[bank_idx] = true;

    // check if bitvector needs to be reset
    bool all_done = std::all_of(ba_bits.begin(), ba_bits.end(), [] (bool x) { return x; });
    if (all_done)
        std::fill(ba_bits.begin(), ba_bits.end(), false);
}

long
BARD::find_victim(long initial_victim_way, long set, pos_iterator pos_begin, pos_iterator pos_end, const champsim::cache_block* current_set)
{
    if (OPT_BARD_MODE == BARD_MODE_CLEAN_ONLY)
        return initial_victim_way;

    // If the replacement policy has already selected a victim, check if it is ok:
    bard_victim_data victim;
    if (initial_victim_way >= 0)
    {
        set_victim_data(victim, initial_victim_way, current_set);

        // If the line is dirty and it improves BLP, then we can also exit early:
        if (victim.dirty)
        {
            bool ok = !bank_write_bitvec[victim.channel][victim.bank_idx];

            if (ok)
            {
                check_if_writeback_is_redundant(victim);
                return initial_victim_way;
            }
        }
        else
        {
            // do not need to do anything if victim is clean
            return initial_victim_way;
        }
    }

    // Otherwise, try to find an entry that improves BLP:
    victim = select_dirty_line(pos_begin, pos_end, current_set);

    if (victim.way_id < 0)
    {
        return initial_victim_way;
    }
    else
    {
        ++s_non_lru_evicts;
        check_if_writeback_is_redundant(victim);
        return victim.way_id;
    }
}

long
BARD::find_eager_writeback(long set, pos_iterator pos_begin, pos_iterator pos_end, const champsim::cache_block* current_set)
{
    if (OPT_BARD_MODE == BARD_MODE_EVICT_ONLY)
        return -1;

    bard_victim_data victim = select_dirty_line(pos_begin, pos_end, current_set);

    if (victim.way_id >= 0)
    {
        ++s_eager_writebacks;
        check_if_writeback_is_redundant(victim);
    }
    return victim.way_id;
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
BARD::select_dirty_line(pos_iterator pos_begin, pos_iterator pos_end, const champsim::cache_block* current_set)
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

        cand.priority = !bank_write_bitvec[cand.channel][cand.bank_idx];

        bool cmp = (evict_lower_positions && cand.pos < victim.pos) 
                    || (!evict_lower_positions && cand.pos > victim.pos);
        if (cand.priority && (victim.way_id < 0 || cmp))
            victim = std::move(cand);
    }

    return victim;
}

void
BARD::check_if_writeback_is_redundant(const bard_victim_data& v)
{
    const auto& dc = cache->dram->channels[v.channel];
    if (dc->does_bank_have_pending_write(v.bank_idx))
        s_redundant_writebacks++;
}

void
cache_set_copy_way_contents_and_clean_source(const champsim::cache_block* _current_set, long from, long to)
{
    auto* current_set = const_cast<champsim::cache_block*>(_current_set);
    current_set[to] = current_set[from];
    current_set[from].dirty = false;
}
