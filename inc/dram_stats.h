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

    uint32_t reads =0;
    uint32_t writes =0;
    uint32_t activates =0;
    uint32_t precharges =0;
    
    uint32_t read_row_hits =0;
    uint32_t write_row_hits =0;

    uint64_t wq_full =0;
};

dram_stats operator-(dram_stats lhs, dram_stats rhs);

#endif
