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
        size_t _channels,
        size_t _bankgroups,
        size_t _banks,
        size_t _rows,
        size_t _columns,
        size_t _llc_sets)
    :channels(_channels),
    bankgroups(_bankgroups),
    banks(_banks),
    rows(_rows),
    columns(_columns),
    ch_width(ilog2(_channels)),
    bg_width(ilog2(_bankgroups)),
    ba_width(ilog2(_banks)),
    row_width(ilog2(_rows)),
    llc_sets(_llc_sets),
    llc_set_width(ilog2(_llc_sets))
{
    size_t column_width = ilog2(columns);

    if (mapping_name[0] == 'p')
    {
        enable_permutation = true;
        mapping_name = mapping_name.substr(1);
    }

    if (mapping_name.find("mop") != std::string::npos)
    {
        size_t mop_size = std::stoi(mapping_name.substr(3));
        
        ch_offset = ilog2(mop_size);
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

champsim::address
DRAM_ADDRESS_MAPPER::set_bank_idx_of_address(champsim::address a, size_t idx) const
{
    uint64_t _a{a.to<uint64_t>()};
    _a &= ~(((1L << (bg_width+ba_width))-1) << (bg_offset+LOG2_BLOCK_SIZE));  // Clear out bank idx
    _a |= (idx << (bg_offset+LOG2_BLOCK_SIZE));
    return champsim::address{_a};
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
