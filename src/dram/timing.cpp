/*
 *  author: Suhas Vittal
 *  date:   24 March 2025
 * */

DRAM_TIMING::DRAM_TIMING(std::string_view dram_type, champsim::chrono::picoseconds mc_period)
{
    // Initialize dram timing:
    auto ckcast = [freq=1.0/static_cast<double>(mc_period)]
                    (double ns) { return static_cast<int>(ceil(t_ns/freq)); };

    // These are nCK values: for some odd reason champsim wants to use
    // picoseconds instead :(
    int _tRP, _tRCD, _CL, _CWL, _tRAS, _tRTP, _tWR,
           _tCCD_S, _tCCD_S_WR, _tCCD_S_WTR,
           _tCCD_L, _tCCD_L_WR, _tCCD_L_WTR, _tCCD_RTW,
           _tRRD_S, _tRRD_L, _tFAW;

    if (dram_type == "4800")
    {
        BL = 16;

        _tRP = 39;
        _tRCD = 39;
        _CL = 40;
        _CWL = CL-2;
        _tRAS = ckcast(32.0);
        _tRTP = std::max(12, ckcast(7.5));
        _tWR = ckcast(30.0);

        _tCCD_S = 8;
        _tCCD_S_WR = 8;
        _tCCD_S_WTR = CWL + BL/2 + std::max(4, ckcast(2.5));

        _tCCD_L = std::max(8, ckcast(5.0));
        _tCCD_L_WR = std::max(32, ckcast(20.0));
        _tCCD_L_WTR = CWL + BL/2 + std::max(16, ckcast(10.0));

        _tCCD_RTW = (CL-CWL) + BL/2 + 4;

        _tRRD_S = 8;
        _tRRD_L = std::max(8, ckcast(5.0));
        _tFAW = std::max(32, ckcast(13.333));
    }
    else if (dram_type == "ddr3_1600")
    {
        BL = 8;

        _tRP = 11;
        _tRCD = 11;
        _CL = 11;
        _CWL = 8;
        _tRAS = ckcast(35.0);
        _tRTP = std::max(4, ckcast(7.5));
        _tWR = ckcast(15.0);

        _tCCD_S = 4;
        _tCCD_S_WR = 4;
        _tCCD_S_WTR = CWL + BL/2 + std::max(4, ckcast(7.5));
        
        _tCCD_L = tCCD_S;
        _tCCD_L_WR = tCCD_S_WR;
        _tCCD_L_WTR = tCCD_S_WTR;

        _tCCD_RTW = (CL-CWL) + BL/2 + 1;
        
        _tRRD_S = std::max(4, ckcast(7.5));
        _tRRD_L = tRRD_S;
        _tFAW = ckcast(20.0);
    }
    else
    {
        std::cerr << "unrecognized dram type \"" << dram_type << "\" -- exiting" << std::endl;
        exit(1);
    }

    // Convert all nCK values to picoseconds:
    auto pscast = [p=mc_period] (int tck) { return tck * p; };

    tRP = pscast(_tRP);
    tRCD = pscast(_tRCD);
    CL = pscast(_CL);
    CWL = pscast(_CWL);
    tRAS = pscast(_tRAS);
    tRTP = pscast(_tRTP);
    tWR = pscast(_tWR);

    tCCD_S = pscast(_tCCD_S);
    tCCD_S_WR = pscast(_tCCD_S_WR);
    tCCD_S_WTR = pscast(_tCCD_S_WTR);

    tCCD_L = pscast(_tCCD_L);
    tCCD_L_WR = pscast(_tCCD_L_WR);
    tCCD_L_WTR = pscast(_tCCD_L_WTR);

    tCCD_RTW = pscast(_tCCD_RTW);

    tRRD_S = pscast(_tRRD_S);
    tRRD_L = pscast(_tRRD_L);
    tFAW = pscast(_tFAW);

    tRFC = 410'000;
    tREFI = 3'906'250;  // 32e6 / 8192 (this is ns -- converted to ps in value).
}
