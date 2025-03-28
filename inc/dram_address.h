/*
 *  author: Suhas Vittal
 *  date:   24 March 2025
 * */

#ifndef DRAM_ADDRESS_h
#define DRAM_ADDRESS_h

#include "address.h"

#include <cstddef>

template <class T>
inline constexpr size_t ilog2(T x)
{
    size_t lg = 0;
    while (x > 1)
    {
        x >>= 1;
        ++lg;
    }
    return lg;
}

class DRAM_ADDRESS_MAPPER
{
public:
    bool enable_permutation =false;

    const size_t channels;
    const size_t bankgroups;
    const size_t banks;
    const size_t rows;
    const size_t columns;
private:
    size_t ch_offset, ch_width;
    size_t bg_offset, bg_width;
    size_t ba_offset, ba_width;
    size_t row_offset, row_width;

    const size_t llc_sets;
    const size_t llc_set_width;
public:
    DRAM_ADDRESS_MAPPER(std::string mapping_name, size_t channels, size_t bankgroups, size_t banks, size_t rows, size_t columns, size_t llc_sets);
    DRAM_ADDRESS_MAPPER(const DRAM_ADDRESS_MAPPER&) =default;

    size_t channel(champsim::address) const;
    size_t bankgroup(champsim::address) const;
    size_t bank(champsim::address) const;
    size_t row(champsim::address) const;

    size_t bank_idx(champsim::address) const;

    champsim::address set_bank_idx_of_address(champsim::address, size_t bank_idx) const;

    void print_address_mapping() const;
};

#endif  // DRAM_ADDRESS_h
