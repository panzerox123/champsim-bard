/*
 *  author: Suhas Vittal
 *  date:   23 March 2025
 * */

#include "dram/controller.h"

#include <iostream>
#include <iomanip>

MEMORY_CONTROLLER::MEMORY_CONTROLLER(champsim::chrono::picoseconds mc_period,
        std::vector<channel_type*>&& ul,
        std::string dram_type,
        std::string address_mapping_name,
        std::size_t rq_size,
        std::size_t wq_size,
        std::size_t num_channels,
        std::size_t num_bankgroups,
        std::size_t num_banks,
        std::size_t num_rows,
        std::size_t num_columns)
    :champsim::operable(mc_period),
    address_mapper(address_mapping_name, num_channels, num_bankgroups, num_banks, num_rows, num_columns),
    dram_timing(dram_type, mc_period),
    channels(num_channels, DRAM_CHANNEL(mc_period, rq_size, wq_size, num_bankgroups, num_banks, num_rows, address_mapper, dram_timing)),
    num_channels(num_channels),
    num_bankgroups(num_bankgroups),
    num_banks(num_banks),
    num_rows(num_rows),
    queues(std::move(ul))
{}

void
MEMORY_CONTROLLER::initialize()
{
    auto list_timing = [this] (std::string name, champsim::chrono::duration t)
    {
        fmt::print("{}\t{}ns\n", name, t * 1e-3);
    };

#define LIST(x) list_timing("#x", dram_timing.##x)

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
}

long
MEMORY_CONTROLLER::operate()
{
    long progress{0};

    initiate_requests();
    for (auto ch : channels)
        progress += ch._operate();
    
    return progress;
}

void
MEMORY_CONTROLLER::begin_phase()
{
    for (size_t i = 0; i < channels.size(); i++)
    {
        DRAM_CHANNEL::stats_type new_stats;
        new_stats.name = "Channel " + std::to_string(i);
        channels[i].sim_stats = new_stats;
        channels[i].warmup = warmup;
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
        chan.end_phase(cpu);
}

void
MEMORY_CONTROLLER::print_deadlock()
{
    for (auto& chan : channels)
        chan.print_deadlock();
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
MEMORY_CONTROLLER::add_rq(const request_type& pkt, champsim::channel* ul)
{
    auto& channel = channels[address_mapping.get_channel(packet.address)];

    auto rq_it = std::find_if_not(std::begin(channel.RQ), std::end(channel.RQ), [] (const auto& pkt) { return pkt.has_value(); });
    if (rq_it != std::end(channel->RQ))
    {
        *rq_it = DRAM_CHANNEL::request_type{packet};
        rq_it->value().forward_checked = false;
        rq_it->value().scheduled = false;
        rq_it->value().ready_time = current_time;
        if (packet.response_requested)
            rq_it->value().to_return = {&ul->returned};

        return true;
    }

    return false;
}

bool
MEMORY_CONTROLLER::add_wq(const request_type& pkt)
{
    auto& channel = channels[address_mapping.get_channel(packet.address)];

    // search for the empty index
    auto wq_it = std::find_if_not(std::begin(channel.WQ), std::end(channel.WQ), [](const auto& pkt) { return pkt.has_value(); }
    if (wq_it != std::end(channel.WQ))
    {
        *wq_it = DRAM_CHANNEL::request_type{packet};
        wq_it->value().forward_checked = false;
        wq_it->value().scheduled = false;
        wq_it->value().ready_time = current_time;

        return true;
    }

    ++channel.sim_stats.WQ_FULL;
    return false;
}
