/*
 *  author: Suhas Vittal
 *  date:   24 March 2025
 * */

#include "dram/address.h"

#include <iostream>

DRAM_ADDRESS_MAPPER::DRAM_ADDRESS_MAPPER(
        std::string mapping_name,
        size_t channels,
        size_t bankgroups,
        size_t banks,
        size_t rows,
        size_t columns)
    :channels(channels),
    bankgroups(bankgroups),
    banks(banks),
    rows(rows),
    columns(columns),
    ch_width(ilog2(channels)),
    bg_width(ilog2(bankgroups)),
    ba_width(ilog2(banks)),
    row_width(ilog2(rows))
{
    size_t column_width = ilog2(columns);

    if (mapping_name.starts_with("mop"))
    {
        size_t mop_width = std::stoi(mapping_name.substr(3));
        
        ch_offset = mop_width;
        bg_offset = ch_offset + ch_width;
        ba_offset = bg_offset + bg_width;
        row_offset = ch_width + bg_width + ba_width + column_width;
    }
    else if (mapping_name == "zen")
    {
        ch_offset = 0;
        bg_offset = ch_width + 1;
        ba_offset = bg_offset + bg_width;
        row_offset = ch_width + bg_width + ba_width + column_width;
    }
    else
    {
        std::cerr << "unknown address mapping: " << mapping_name << std::endl;
        exit(1);
    }
}

size_t
DRAM_ADDRESS_MAPPING::channel(champsim::address x) const
{
    return x.slice(dynamic_extent{ch_offset+ch_width, ch_offset}).to();
}

size_t
DRAM_ADDRESS_MAPPING::bankgroup(champsim::address x) const
{
    return x.slice(dynamic_extent{bg_offset+bg_width, bg_offset}).to();
}

size_t
DRAM_ADDRESS_MAPPING::bank(champsim::address x) const
{
    return x.slice(dynamic_extent{ba_offset+ba_width, ba_offset}).to();
}

size_t
DRAM_ADDRESS_MAPPING::row(champsim::address x) const
{
    return x.slice(dynamic_extent{row_offset+row_width, row_offset}).to();
}

size_t
DRAM_ADDRESS_MAPPING::bank_idx(champsim::address x) const
{
    return bankgroup(x) * banks + banks(x);
}
