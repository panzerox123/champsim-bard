/*
 *  author: Suhas Vittal
 *  date:   23 March 2025
 * */

#ifndef DRAM_CONTROLLER_h
#define DRAM_CONTROLLER_h

#include "dram_address.h"
#include "dram_channel.h"
#include "dram_timing.h"

#include <memory>

class MEMORY_CONTROLLER : public champsim::operable
{
public:
    DRAM_ADDRESS_MAPPER address_mapper;
    DRAM_TIMING dram_timing;

    std::vector<std::shared_ptr<DRAM_CHANNEL>> channels;

    const std::string dram_type;

    const size_t num_channels;
    const size_t num_bankgroups;
    const size_t num_banks;
    const size_t num_rows;
    const size_t num_columns;
private:
    using channel_type = champsim::channel;
    using request_type = typename channel_type::request_type;
    using response_type = typename channel_type::response_type;

    std::vector<channel_type*> queues;
public:
    MEMORY_CONTROLLER(champsim::chrono::picoseconds mc_period, std::vector<channel_type*>&& ul,
                        std::string dram_type, std::string address_mapping_name,
                        std::size_t rq_size, std::size_t wq_size,
                        std::size_t num_channels, std::size_t num_bankgroups, std::size_t num_banks, std::size_t num_rows, std::size_t num_columns,
                        std::size_t llc_sets);

    void initialize() final;
    long operate() final;
    void begin_phase() final;
    void end_phase(unsigned cpu) final;
    void print_deadlock() final;

    champsim::data::bytes size() const;
private:
    void initiate_requests();
    bool add_rq(const request_type& packet, champsim::channel* ul);
    bool add_wq(const request_type& packet);
};

#endif   // DRAM_CONTROLLER_h
