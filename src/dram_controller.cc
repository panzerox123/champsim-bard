/*
 *  author: Suhas Vittal
 *  date:   23 March 2025
 * */

#include "champsim.h"
#include "dram_controller.h"
#include "util/bits.h" // for lg2, bitmask
#include "util/span.h"
#include "util/units.h"

#include <algorithm>
#include <iostream>
#include <iomanip>

MEMORY_CONTROLLER::MEMORY_CONTROLLER(champsim::chrono::picoseconds mc_period,
        std::vector<channel_type*>&& ul,
        std::string dram_type,
        std::string address_mapping_name,
        std::size_t rq_size,
        std::size_t wq_size,
        std::size_t _num_channels,
        std::size_t _num_bankgroups,
        std::size_t _num_banks,
        std::size_t _num_rows,
        std::size_t _num_columns,
        std::size_t llc_sets,
        bool baws)
    :champsim::operable(mc_period),
    address_mapper(address_mapping_name, _num_channels, _num_bankgroups, _num_banks, _num_rows, _num_columns, llc_sets),
    dram_timing(dram_type, mc_period),
    num_channels(_num_channels),
    num_bankgroups(_num_bankgroups),
    num_banks(_num_banks),
    num_rows(_num_rows),
    num_columns(_num_columns),
    queues(std::move(ul))
{
    for (size_t i = 0; i < num_channels; i++)
    channels.emplace_back(new DRAM_CHANNEL(mc_period, rq_size, wq_size, i, num_bankgroups, num_banks, num_rows, address_mapper, dram_timing, baws));
}

void
MEMORY_CONTROLLER::initialize()
{
    auto list_timing = [] (std::string name, champsim::chrono::clock::duration t)
    {
        fmt::print("{} :\t {} ns\n", name, t.count() * 1e-3);
    };

#define LIST(x) list_timing(#x, dram_timing.x)

    LIST(CL);
    LIST(CWL);
    LIST(tRP);
    LIST(tRCD);
    LIST(tRAS);
    LIST(tRTP);
    LIST(tWR);

    fmt::print("\n");

    LIST(tCCD_S);
    LIST(tCCD_S_WR);
    LIST(tCCD_S_WTR);
    LIST(tCCD_L);
    LIST(tCCD_L_WR);
    LIST(tCCD_L_WTR);

    LIST(tRRD_S);
    LIST(tRRD_L);
    LIST(tFAW);

#undef LIST

    address_mapper.print_address_mapping();

    fmt::print("dram queue size: R = {} W = {} (low:high is {}:{})\n", channels[0]->RQ.size(), channels[0]->WQ.size(), channels[0]->low_watermark, channels[0]->high_watermark);
}

long
MEMORY_CONTROLLER::operate()
{
    long progress{0};

    initiate_requests();
    for (auto& ch : channels)
        progress += ch->_operate();
    
    return progress;
}

void
MEMORY_CONTROLLER::begin_phase()
{
    for (size_t i = 0; i < channels.size(); i++)
    {
        DRAM_CHANNEL::stats_type new_stats;
        new_stats.name = "Channel " + std::to_string(i);
        channels[i]->sim_stats = new_stats;
        channels[i]->warmup = warmup;
    }

    for (auto* ul : queues)
    {
        channel_type::stats_type ul_new_roi_stats;
        channel_type::stats_type ul_new_sim_stats;
        ul->roi_stats = ul_new_roi_stats;
        ul->sim_stats = ul_new_sim_stats;
    }
}

void
MEMORY_CONTROLLER::end_phase(unsigned cpu)
{
    for (auto& chan : channels)
        chan->end_phase(cpu);
}

void
MEMORY_CONTROLLER::print_deadlock()
{
    for (auto& chan : channels)
        chan->print_deadlock();
}

champsim::data::bytes
MEMORY_CONTROLLER::size() const
{
    return champsim::data::bytes{static_cast<long long>(num_channels*num_bankgroups*num_banks*num_rows*num_columns*BLOCK_SIZE)};
}

void
MEMORY_CONTROLLER::initiate_requests()
{
    // Initiate read requests
    for (auto* ul : queues)
    {
        for (auto q : {std::ref(ul->RQ), std::ref(ul->PQ)})
        {
            auto [begin, end] = champsim::get_span_p(std::cbegin(q.get()), std::cend(q.get()), [ul, this](const auto& pkt) { return this->add_rq(pkt, ul); });
            q.get().erase(begin, end);
        }

        // Initiate write requests
        auto [wq_begin, wq_end] = champsim::get_span_p(std::cbegin(ul->WQ), std::cend(ul->WQ), [this](const auto& pkt) { return this->add_wq(pkt); });
        ul->WQ.erase(wq_begin, wq_end);
    }
}

bool
MEMORY_CONTROLLER::add_rq(const request_type& packet, champsim::channel* ul)
{
    auto& channel = channels[address_mapper.channel(packet.address)];

    auto rq_it = std::find_if_not(std::begin(channel->RQ), std::end(channel->RQ), [] (const auto& pkt) { return pkt.has_value(); });
    if (rq_it != std::end(channel->RQ))
    {
        *rq_it = DRAM_CHANNEL::request_type{packet};
        rq_it->value().forward_checked = false;
        rq_it->value().scheduled = false;
        rq_it->value().ready_time = current_time;
        if (packet.response_requested)
            rq_it->value().to_return = {&ul->returned};

        rq_it->value().install_time = current_time;

        ++channel->sim_stats.read_requests;
        return true;
    }

    ++channel->sim_stats.rq_full;
    return false;
}

bool
MEMORY_CONTROLLER::add_wq(const request_type& packet)
{
    auto& channel = channels[address_mapper.channel(packet.address)];

    // search for the empty index
    auto wq_it = std::find_if_not(std::begin(channel->WQ), std::end(channel->WQ), [](const auto& pkt) { return pkt.has_value(); });
    if (wq_it != std::end(channel->WQ))
    {
        *wq_it = DRAM_CHANNEL::request_type{packet};
        wq_it->value().forward_checked = false;
        wq_it->value().scheduled = false;
        wq_it->value().ready_time = current_time;

        wq_it->value().install_time = current_time;

        ++channel->sim_stats.write_requests;
        return true;
    }

    ++channel->sim_stats.wq_full;
    return false;
}
