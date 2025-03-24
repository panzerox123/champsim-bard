/*
 *  author: Suhas Vittal
 *  date:   24 March 2025
 * */

#ifndef DRAM_TIMING_h
#define DRAM_TIMING_h

#include "chrono.h"

#include <cstddef>
#include <string_view>

struct DRAM_TIMING
{
    champsim::chrono::clock::duration
        // Bank timings:
        tRP, tRCD, CL, CWL, tRAS, tRTP, tWR, burst,
        // Bankgroup timings:
        tCCD_S, tCCD_L, tCCD_S_WR, tCCD_L_WR,
        tCCD_S_WTR, tCCD_L_WTR, tCCD_RTW,
        tRRD_S, tRRD_L, tFAW,
        // Refresh:
        tRFC, tREFI;

    int BL;

    DRAM_TIMING(std::string_view dram_type, champsim::chrono::picoseconds mc_period);
    DRAM_TIMING(const DRAM_TIMING&) =default;
};

#endif  // DRAM_TIMING_h
