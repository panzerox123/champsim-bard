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
    lhs.tot_read_occu_pre_drain -= rhs.tot_read_occu_pre_drain;
    lhs.tot_read_occu_post_drain -= rhs.tot_read_occu_post_drain;
    lhs.tot_write_occu_pre_drain -= rhs.tot_write_occu_pre_drain;
    lhs.tot_write_occu_post_drain -= rhs.tot_write_occu_post_drain;
    return lhs;
}
