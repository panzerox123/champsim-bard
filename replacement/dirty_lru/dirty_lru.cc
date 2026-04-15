/*
 *  Dirty-preferred LRU replacement policy
 *
 *  find_victim rules:
 *    1) Evict the least recently used DIRTY line in the set.
 *    2) If no dirty lines exist, evict the least recently used line (clean).
 */

#include "dirty_lru.h"

#include <algorithm>
#include <cassert>
#include <limits>

dirty_lru::dirty_lru(CACHE* cache) : dirty_lru(cache, cache->NUM_SET, cache->NUM_WAY) {}

dirty_lru::dirty_lru(CACHE* cache, long sets, long ways)
    : replacement(cache),
      NUM_WAY(ways),
      last_used_cycles(static_cast<std::size_t>(sets * ways), 0)
{}

long dirty_lru::find_victim(uint32_t /*triggering_cpu*/, uint64_t /*instr_id*/, long set,
                             const champsim::cache_block* current_set,
                             champsim::address /*ip*/, champsim::address /*full_addr*/,
                             access_type /*type*/)
{
    auto lru_begin = std::next(std::begin(last_used_cycles), set * NUM_WAY);
    auto lru_end   = std::next(lru_begin, NUM_WAY);

    // Pass 1: find the LRU dirty line.
    long   dirty_victim     = -1;
    uint64_t dirty_min_cycle = std::numeric_limits<uint64_t>::max();

    for (long way = 0; way < NUM_WAY; ++way)
    {
        const auto& blk = current_set[way];
        if (!blk.valid || !blk.dirty)
            continue;

        uint64_t used = lru_begin[way];
        if (used < dirty_min_cycle)
        {
            dirty_min_cycle = used;
            dirty_victim    = way;
        }
    }

    if (dirty_victim >= 0)
        return dirty_victim;

    // Pass 2: no dirty lines — fall back to global LRU.
    auto victim = std::min_element(lru_begin, lru_end);
    assert(lru_begin <= victim);
    assert(victim < lru_end);
    return std::distance(lru_begin, victim);
}

void dirty_lru::replacement_cache_fill(uint32_t /*triggering_cpu*/, long set, long way,
                                       champsim::address /*full_addr*/, champsim::address /*ip*/,
                                       champsim::address /*victim_addr*/, access_type /*type*/)
{
    last_used_cycles.at(static_cast<std::size_t>(set * NUM_WAY + way)) = cycle++;
}

void dirty_lru::update_replacement_state(uint32_t /*triggering_cpu*/, long set, long way,
                                         champsim::address /*full_addr*/, champsim::address /*ip*/,
                                         champsim::address /*victim_addr*/, access_type type,
                                         uint8_t hit)
{
    // Do not update LRU position on writeback hits (same convention as lru.cc).
    if (hit && access_type{type} != access_type::WRITE)
        last_used_cycles.at(static_cast<std::size_t>(set * NUM_WAY + way)) = cycle++;
}
