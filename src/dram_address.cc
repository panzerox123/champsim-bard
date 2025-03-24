/*
 *  author: Suhas Vittal
 *  date:   24 March 2025
 * */

#include "dram_address.h"
#include "extent.h"
#include "extent_set.h"

#include <iostream>

DRAM_ADDRESS_MAPPER::DRAM_ADDRESS_MAPPER(
        std::string mapping_name,
        size_t channels,
        size_t bankgroups,
        size_t banks,
        size_t rows,
        size_t columns,
        size_t llc_sets)
    :channels(channels),
    bankgroups(bankgroups),
    banks(banks),
    rows(rows),
    columns(columns),
    ch_width(ilog2(channels)),
    bg_width(ilog2(bankgroups)),
    ba_width(ilog2(banks)),
    row_width(ilog2(rows)),
    llc_sets(llc_sets),
    llc_set_width(ilog2(llc_sets))
{
    size_t column_width = ilog2(columns);

    if (mapping_name[0] == 'p')
    {
        enable_permutation = true;
        mapping_name = mapping_name.substr(1);
    }

    if (mapping_name.find("mop") != std::string::npos)
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
DRAM_ADDRESS_MAPPER::channel(champsim::address x) const
{
    return x.slice(champsim::dynamic_extent{champsim::data::bits{ch_offset+LOG2_BLOCK_SIZE}, ch_width}).to<size_t>();
}

size_t
DRAM_ADDRESS_MAPPER::bankgroup(champsim::address x) const
{
    size_t id = x.slice(champsim::dynamic_extent{champsim::data::bits{bg_offset+LOG2_BLOCK_SIZE}, bg_width}).to<size_t>();
    if (enable_permutation)
        id ^= x.slice(champsim::dynamic_extent{champsim::data::bits{llc_set_width+LOG2_BLOCK_SIZE}, bg_width}).to<size_t>();
    return id;
}

size_t
DRAM_ADDRESS_MAPPER::bank(champsim::address x) const
{
    size_t id = x.slice(champsim::dynamic_extent{champsim::data::bits{ba_offset+LOG2_BLOCK_SIZE}, ba_width}).to<size_t>();
    if (enable_permutation)
        id ^= x.slice(champsim::dynamic_extent{champsim::data::bits{llc_set_width+LOG2_BLOCK_SIZE+bg_width}, ba_width}).to<size_t>();
    return id;
}

size_t
DRAM_ADDRESS_MAPPER::row(champsim::address x) const
{
    return x.slice(champsim::dynamic_extent{champsim::data::bits{row_offset+LOG2_BLOCK_SIZE}, row_width}).to<size_t>();
}

size_t
DRAM_ADDRESS_MAPPER::bank_idx(champsim::address x) const
{
    return bankgroup(x) * banks + bank(x);
}

void
DRAM_ADDRESS_MAPPER::print_address_mapping() const
{
    std::cout << "Address mapping (perm = " << enable_permutation << ") \t";

    size_t n = ilog2(channels*bankgroups*banks*rows*columns);
    for (size_t i = 0; i < n; i++)
    {
        if (i >= ch_offset && i < ch_offset+ch_width)
            std::cout << "ch ";
        else if (i >= bg_offset && i < bg_offset+bg_width)
            std::cout << "bg ";
        else if (i >= ba_offset && i < ba_offset+ba_width)
            std::cout << "ba ";
        else if (i >= row_offset && i < row_offset+row_width)
            std::cout << "ro ";
        else
            std::cout << "co ";
    }
    std::cout << "\n";
}
