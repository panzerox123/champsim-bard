#include "bard_ship.h"

#include <algorithm>
#include <cassert>
#include <random>

#include "champsim.h"

// initialize replacement state
bard_ship::bard_ship(CACHE* cache)
    :replacement(cache),
    NUM_SET(cache->NUM_SET),
    NUM_WAY(cache->NUM_WAY),
    sampler(SAMPLER_SET_FACTOR * NUM_CPUS * static_cast<std::size_t>(NUM_WAY)),
    rrpv_values(static_cast<std::size_t>(NUM_SET * NUM_WAY), RRPV_MAX),
    bard_impl(4, cache->NUM_SET, cache->NUM_WAY, cache->dram, false)
{
    // randomly selected sampler sets
    std::generate_n(std::back_inserter(rand_sets), SAMPLER_SET_FACTOR * NUM_CPUS, std::knuth_b{1});
    std::sort(std::begin(rand_sets), std::end(rand_sets));

    std::generate_n(std::back_inserter(SHCT), NUM_CPUS, []() -> typename decltype(SHCT)::value_type { return {}; });
}

int& bard_ship::get_rrpv(long set, long way) { return rrpv_values.at(static_cast<std::size_t>(set * NUM_WAY + way)); }

// find replacement victim
long bard_ship::find_victim(uint32_t triggering_cpu, uint64_t instr_id, long set, const champsim::cache_block* current_set, champsim::address ip,
                       champsim::address full_addr, access_type type)
{
    const int max_lookup = bard_impl.get_max_eviction_pos();

    auto begin = std::next(rrpv_values.begin(), set*NUM_WAY);
    auto end = std::next(begin, NUM_WAY);

    if (bard_impl.is_sampled_set(set))
    {
        long rand_way = std::rand();
        auto rand_it = std::next(begin, rand_way);
        int r = *rand_it;

        bard_impl.handle_mark(set, rand_way, r, current_set[rand_way].dirty);
    }

    auto v_it = std::max_element(begin, end);
    if (max_lookup < 4 && *v_it < max_lookup)
    {
        // Compute delta from max lookup:
        int d = max_lookup - *v_it;

        // Increment all rrpvs by this much:
        std::for_each(begin, end, [d] (auto& x) { x += d; });
    }

    long victim_way = std::distance(begin, v_it);

    // Check for an alternate candidate:
    if (bard_impl.is_sampled_set(set))
    {
        bard_impl.handle_recapture(set, victim_way, BARD::RecaptureType::EVICT);
    }
    else
    {
        if (current_set[victim_way].dirty)
        {
            victim_way = bard_impl.find_victim(victim_way, set, begin, end, current_set);
        }
        else
        {
            long shadow_way = bard_impl.find_eager_writeback(set, begin, end, current_set);
            if (shadow_way >= 0)
                cache_set_copy_way_contents_and_clean_source(current_set, shadow_way, victim_way);
        }
    }

    // Increment RRPVs one final time
    v_it = std::next(begin, victim_way);
    int d = RRPV_MAX - *v_it;
    std::for_each(begin, end, 
            [d] (auto& x) 
            { 
                x += d; 
                if (x > RRPV_MAX)
                    x = RRPV_MAX;
            });

    if (current_set[victim_way].dirty)
        bard_impl.handle_writeback(current_set[victim_way].address);

    return victim_way;
}

// called on every cache hit and cache fill
void bard_ship::update_replacement_state(uint32_t triggering_cpu, long set, long way, champsim::address full_addr, champsim::address ip,
                                    champsim::address victim_addr, access_type type, uint8_t hit)
{
    using namespace champsim::data::data_literals;

    int initial_rrpv = hit ? 0 : bard_impl.get_max_eviction_pos()-1;
    if (initial_rrpv < RRPV_MIN)  // Can occur when max lookup = 0
        initial_rrpv = RRPV_MIN;

    // handle writeback access
    if (access_type{type} == access_type::WRITE)
    {
        if (!hit)
            get_rrpv(set, way) = initial_rrpv;

        return;
    }

    // update sampler
    auto s_idx = std::find(std::begin(rand_sets), std::end(rand_sets), set);
    if (s_idx != std::end(rand_sets)) {
        auto s_set_begin = std::next(std::begin(sampler), std::distance(std::begin(rand_sets), s_idx));
        auto s_set_end = std::next(s_set_begin, NUM_WAY);

        // check hit
        auto match = std::find_if(s_set_begin, s_set_end, [addr = full_addr, shamt = champsim::data::bits{8 + champsim::lg2(NUM_WAY)}](auto x) {
            return x.valid && x.address.slice_upper(shamt) == addr.slice_upper(shamt);
        });
        if (match != s_set_end) {
            auto SHCT_idx = match->ip.slice_lower<32_b>().to<std::size_t>() % SHCT_PRIME;
            SHCT[triggering_cpu][SHCT_idx]--;

            match->used = true;
        } else {
            match = std::min_element(s_set_begin, s_set_end, [](auto x, auto y) { return x.last_used < y.last_used; });

            if (match->used) {
                auto SHCT_idx = match->ip.slice_lower<32_b>().to<std::size_t>() % SHCT_PRIME;
                SHCT[triggering_cpu][SHCT_idx]++;
            }

            match->valid = true;
            match->address = full_addr;
            match->ip = ip;
            match->used = false;
        }

        // update LRU state
        match->last_used = access_count++;
    }

    if (hit)
    {
        get_rrpv(set, way) = 0;
    }
    else
    {
        // SHIP prediction
        auto SHCT_idx = ip.slice_lower<32_b>().to<std::size_t>() % SHCT_PRIME;

        get_rrpv(set, way) = initial_rrpv;
        if (SHCT[triggering_cpu][SHCT_idx].is_max())
            get_rrpv(set, way) = RRPV_MAX;
    }
}
