#include "dram_stats.h"

dram_stats operator-(dram_stats lhs, dram_stats rhs)
{
    lhs.reads -= rhs.reads;
    lhs.writes -= rhs.writes;
    lhs.activates -= rhs.activates;
    lhs.precharges -= rhs.precharges;
    lhs.read_row_hits -= rhs.read_row_hits;
    lhs.write_row_hits -= rhs.write_row_hits;
    lhs.wq_full -= rhs.wq_full;
    return lhs;
}
