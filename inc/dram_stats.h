#ifndef DRAM_STATS_H
#define DRAM_STATS_H

#include <cstdint>
#include <string>

struct dram_stats
{
    std::string name{};
    /*
    long dbus_cycle_congested{};
    uint64_t dbus_count_congested = 0;
    uint64_t refresh_cycles = 0;
    unsigned WQ_ROW_BUFFER_HIT = 0, WQ_ROW_BUFFER_MISS = 0, RQ_ROW_BUFFER_HIT = 0, RQ_ROW_BUFFER_MISS = 0, WQ_FULL = 0;
    */
    
    uint32_t read_requests =0;
    uint32_t write_requests =0;

    uint32_t reads =0;
    uint32_t writes =0;
    uint32_t activates =0;
    uint32_t precharges =0;
    
    uint32_t read_row_hits =0;
    uint32_t write_row_hits =0;

    uint32_t num_write_drains =0;
    uint32_t num_forced_write_drains =0;
    uint64_t tot_time_in_write_mode =0;
    uint64_t tot_write_imbalance =0;
    uint64_t tot_read_occu_pre_drain =0;
    uint64_t tot_read_occu_post_drain =0;

    uint64_t tot_read_latency =0;

    uint64_t rq_full =0;
    uint64_t wq_full =0;
};

dram_stats operator-(dram_stats lhs, dram_stats rhs);

#endif
