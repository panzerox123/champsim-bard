#include "vwq_lru.h"

#include <algorithm>
#include <cassert>

vwq_lru::vwq_lru(CACHE* cache) : vwq_lru(cache, cache->NUM_SET, cache->NUM_WAY) {}

vwq_lru::vwq_lru(CACHE* cache, long sets, long ways)
    :replacement(cache),
    NUM_WAY(ways),
    last_used_cycles(static_cast<std::size_t>(sets * ways), 0),
    block_ref(cache->block),
    NUM_VWQ_WAY(ways/4)
{}

long vwq_lru::find_victim(uint32_t triggering_cpu, uint64_t instr_id, long set, const champsim::cache_block* current_set, champsim::address ip,
                      champsim::address full_addr, access_type type)
{
  auto begin = std::next(std::begin(last_used_cycles), set * NUM_WAY);
  auto end = std::next(begin, NUM_WAY);

  // Find the way whose last use cycle is most distant
  auto victim = std::min_element(begin, end);
  assert(begin <= victim);
  assert(victim < end);
  return std::distance(begin, victim);
}

void vwq_lru::replacement_cache_fill(uint32_t triggering_cpu, long set, long way, champsim::address full_addr, champsim::address ip, champsim::address victim_addr,
                                 access_type type)
{
  // Mark the way as being used on the current cycle
  last_used_cycles.at((std::size_t)(set * NUM_WAY + way)) = cycle++;

  update_vwq_state(set);
}

void vwq_lru::update_replacement_state(uint32_t triggering_cpu, long set, long way, champsim::address full_addr, champsim::address ip,
                                   champsim::address victim_addr, access_type type, uint8_t hit)
{
    // Mark the way as being used on the current cycle
    if (hit && access_type{type} != access_type::WRITE) // Skip this for writeback hits
    {
        last_used_cycles.at((std::size_t)(set * NUM_WAY + way)) = cycle++;
        update_vwq_state(set);
    }
}

void
vwq_lru::update_vwq_state(long set)
{
    auto begin = std::next(std::begin(last_used_cycles), set*NUM_WAY);
    auto end = std::next(begin, NUM_WAY);

    size_t ii = 0;
    for (auto it = begin; it != end; it++)
    {
        size_t lru_pos = std::count_if(begin, end,
                            [t=*it] (auto x) { return t > x; });
        auto way = std::next(std::begin(block_ref), set*NUM_WAY + ii);
        way->vwq_can_use = (lru_pos < NUM_VWQ_WAY);
        ++ii;
    }
}
