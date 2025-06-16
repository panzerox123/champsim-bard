#include "eager_lru.h"

#include <algorithm>
#include <cassert>

eager_lru::eager_lru(CACHE* cache) : eager_lru(cache, cache->NUM_SET, cache->NUM_WAY) {}

eager_lru::eager_lru(CACHE* cache, long sets, long ways)
    :replacement(cache),
    NUM_WAY(ways),
    last_used_cycles(static_cast<std::size_t>(sets * ways), 0),
    block_ref(cache->block),
    NUM_VWQ_WAY(ways/4)
{}

long eager_lru::find_victim(uint32_t triggering_cpu, uint64_t instr_id, long set, const champsim::cache_block* current_set, champsim::address ip,
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

void eager_lru::replacement_cache_fill(uint32_t triggering_cpu, long set, long way, champsim::address full_addr, champsim::address ip, champsim::address victim_addr,
                                 access_type type)
{
  // Mark the way as being used on the current cycle
  last_used_cycles.at((std::size_t)(set * NUM_WAY + way)) = cycle++;

  update_eager_state(set);
}

void eager_lru::update_replacement_state(uint32_t triggering_cpu, long set, long way, champsim::address full_addr, champsim::address ip,
                                   champsim::address victim_addr, access_type type, uint8_t hit)
{
    // Mark the way as being used on the current cycle
    if (hit && access_type{type} != access_type::WRITE) // Skip this for writeback hits
    {
        last_used_cycles.at((std::size_t)(set * NUM_WAY + way)) = cycle++;
        update_eager_state(set);
    }
}

void
eager_lru::update_eager_state(long set)
{
    auto begin = std::next(std::begin(last_used_cycles), set*NUM_WAY);
    auto end = std::next(begin, NUM_WAY);

    // First reset the `vwq_can_use` variable
    for (int i = 0; i < NUM_WAY; i++)
        block_ref[set*NUM_WAY + i].vwq_can_use = false;

    // Only set it for LRU
    auto lru = std::min_element(begin, end);
    long way = std::distance(begin, lru);
    block_ref[set*NUM_WAY + way].vwq_can_use = true;
}
