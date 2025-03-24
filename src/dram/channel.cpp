/*
 *  author: Suhas Vittal
 *  date:   23 March 2025
 * */

#include "dram/channel.h"

#include <cmath>
#include <iostream>

DRAM_CHANNEL::request_type::request_type(const typename champsim::channel::request_type& req)
    : pf_metadata(req.pf_metadata), address(req.address), v_address(req.address), data(req.data), instr_depend_on_me(req.instr_depend_on_me)
{
  asid[0] = req.asid[0];
  asid[1] = req.asid[1];
}

DRAM_CHANNEL::DRAM_CHANNEL(
        champsim::chrono::picoseconds mc_period,
        std::size_t rq_size,
        std::size_t wq_size,
        std::size_t num_bankgroups,
        std::size_t num_banks,
        std::size_t num_rows,
        const DRAM_ADDRESS_MAPPING& am,
        const DRAM_TIMING& timing)
    :champsim::operable(mc_period),
    RQ(rq_size),
    WQ(wq_size),
    banks(num_bankgroups*num_banks, BANK_DATA{}),
    low_watermark(wq_size/4),
    high_watermark(3 * (wq_size/4)),
    num_bankgroups(num_bankgroups),
    num_banks(num_banks),
    num_rows(num_rows),
    address_mapper(am),
    dram_timing(timing)
{}

DRAM_CHANNEL::cmd_output_type
DRAM_CHANNEL::find_ready_request()
{
    cmd_output_type out;

    // First check active buffer for any issuable commands:
    for (auto it = banks.begin(); it != banks.end(); it++)
    {
        if (!it->active_request.has_value())
            continue;

        // Check if the request is issuable:
        auto& [is_read, req_it] = it->active_request.value();
        if ((is_read && current_time >= it->state.read_ok) || (!is_read && current_time >= it->state.write_ok))
        {
            DRAM_COMMAND::TYPE cmd_type = is_read ? DRAM_COMMAND::TYPE::READ : DRAM_COMMAND::TYPE::WRITE;
            DRAM_COMMAND ready_cmd{cmd_type, req_it->value().address};

            if (out.first.type == DRAM_COMMAND::TYPE::INVALID
                || (out.second->value().ready_time > req_it->value().ready_time()))
            {
                out = cmd_output_type{ready_cmd, req_it};
            }
        }
    }

    if (out.first.type != DRAM_COMMAND::TYPE::INVALID)
        return out;

    // If nothing could be done, schedule reads and writes:
    auto& q = write_mode ? WQ : RQ;
    for (auto it = q.begin(); it != q.end(); it++)
    {
        if (!it->has_value())
            continue;

        const auto& req = it->value();
        if (req.scheduled)
            continue;

        DRAM_COMMAND ready_cmd{};
        ready_cmd.address = req.address;

        size_t b_idx = address_mapper.bank_idx(req.address);
        
        const auto& b = banks.at(b_idx);
        
        // Cannot schedule anything if the bank has an active request:
        if (b.active_request.has_value())
            continue;

        if (b.state.open_row.has_value())
        {
            if (b.state.open_row.value() == address_mapper.row(req.address))
            {
                if (write_mode && current_time >= b.state.write_ok)
                    ready_cmd.type = DRAM_COMMAND::TYPE::WRITE;
                else if (!write_mode && current_time >= b.state.read_ok)
                    ready_cmd.type = DRAM_COMMAND::TYPE::READ;
            }
            else
            {
                bool no_pending_row_hits = std::none_of(std::next(it), q.end(),
                                                [this, b_idx, row=b.state.open_row.value()]
                                                (const auto& e)
                                                {
                                                    if (!e.has_value() || e.value().scheduled)
                                                        return false;
                                                    if (this->address_mapper.bank_idx(e.value().address) != b_idx)
                                                        return false;
                                                    return this->address_mapper.row(e.value().address) == row;
                                                });
                if (no_pending_row_hits && current_time >= b.state.pre_ok)
                    ready_cmd.type = DRAM_COMMAND::TYPE::PRECHARGE;
            }
        }
        else if (current_time >= b.state.act_ok && faw.size() < 4)
        {
            ready_cmd.type = DRAM_COMMAND::TYPE::ACTIVATE;
        }

        if (ready_cmd.type != DRAM_COMMAND::TYPE::INVALID &&
            (out.first.type == DRAM_COMMAND::TYPE::INVALID || out.second->value().ready_time < req.ready_time))
        {
            out = cmd_output_type{ready_cmd, it};
        }
    }

    return out;
}

void update_dram_timing(champsim::chrono::clock::time_point& t, champsim::chrono::clock::duration delta)
{
    t = std::max(t, current_time + delta);
}

long
DRAM_CHANNEL::schedule_ready_request()
{
    long progress{0};

    const auto& [cmd, q_it] = find_ready_request();
    if (cmd.type == DRAM_COMMAND::TYPE::INVALID)
        return progress;

    ++progress;

    size_t b_idx = address_mapper.bank_idx(cmd.address);
    size_t bg = address_mapper.bankgroup(cmd.address);
    auto& b = banks[b_idx];

    if (cmd.type == DRAM_COMMAND::TYPE::READ || cmd.type == DRAM_COMMAND::TYPE::WRITE)
    {
        const bool is_read = cmd.type == DRAM_COMMAND::TYPE::READ;
            
        q_it->value().scheduled = true;
        q_it->value().ready_time = current_time + (is_read ? (dram_timing.CL + dram_timing.BL/2) 
                                                           : (dram_timing.CWL + dram_timing.BL/2));

        b.active_request.reset();
        b.state.next_cas_is_row_hit = true;
        update_dram_timing(b.state.pre_ok, is_read ? tRTP : (CWL+BL/2+tWR));
    }
    else if (cmd.type == DRAM_COMMAND::TYPE::ACTIVATE)
    {
        // Update bank state and active entry;
        b.active_request = BANK_DATA::active_entry_type{!write_mode, q_it};
        b.state.open_row = address_mapper.row(cmd.address);

        update_dram_timing(b.state.read_ok, dram_timing.tRCD);
        update_dram_timing(b.state.write_ok, dram_timing.tRCD);
        update_dram_timing(b.state.pre_ok, dram_timing.tRAS);

        // Update faw:
        faw.push_back(current_time + tFAW);
    }
    else  // Precharge:
    {
        b.state.open_row.reset();
        b.state.next_cas_is_row_hit = false;
        update_dram_timing(b.state.act_ok, dram_timing.tRP);
    }

    // Update dram bankgroup state:
    size_t ii = 0;
    for (size_t i = 0; i < num_bankgroups; i++)
    {
        bool same_bankgroup = (i == bg);
        for (size_t j = 0; j < num_banks; j++)
        {
            auto& bb = banks[ii]; 

            if (cmd.type == DRAM_COMMAND::TYPE::READ)
            {
                update_dram_timing(bb.state.read_ok, same_bankgroup ? dram_timing.tCCD_L : dram_timing.tCCD_S);
                update_dram_timing(bb.state.write_ok, dram_timing.tCCD_RTW);
            }
            else if (cmd.type == DRAM_COMMAND::TYPE::WRITE)
            {
                update_dram_timing(bb.state.read_ok, same_bankgroup ? dram_timing.tCCD_L_WTR : dram_timing.tCCD_S_WTR);
                update_dram_timing(bb.state.write_ok, same_bankgroup ? dram_timing.tCCD_L_WR : dram_timing.tCCD_S_WR);
            }
            else if (cmd.type == DRAM_COMMAND::TYPE::ACTIVATE)
            {
                update_dram_timing(bb.state.act_ok, same_bankgroup ? dram_timing.tRRD_L : dram_timing.tRRD_S);
            }
            // No bankgroup timings for precharge
            ++ii;
        }
    }

    return progress;
}

long
DRAM_CHANNEL::handle_refresh()
{
    return 0;  // Honestly refresh complicates the design unnecessarily for
               // microarchitecture research. Currently, refresh is not
               // supported, but tRFC and tREFI are available if needs
               // to be implemented.
}

long
DRAM_CHANNEL::complete_requests()
{
    long progress = 0;

    for (auto it = WQ.begin(); it != WQ.end(); it++)
    {
        if (!it->has_value())
            continue;

        if (it->value().scheduled && current_time >= it->value().ready_time)
            it->reset();
    }

    for (auto it = RQ.begin(); it != RQ.end(); it++)
    {
        if (!it->has_value())
            continue;

        if (it->value().scheduled && current_time >= it->value().ready_time)
        {
            response_type response{r_it->value().address,
                                    r_it->value().v_address,
                                    r_it->value().data,
                                    r_it->value().pf_metadata,
                                    r_it->value().instr_depend_on_me};

            for (auto* ret : r_it->value().to_return)
                ret->push_back(response);

            it->reset();
            ++progress;
        }
    }
    
    return progress;
}

void
DRAM_CHANNEL::check_write_collision()
{
    for (auto w_it = WQ.begin(); w_it != WQ.end(); w_it++)
    {
        if (w_it->has_value() && !w_it->value().forward_checked)
        {
            auto address = w_it->value().address;
            auto checker = [match=address.slice_upper(LOG2_BLOCK_OFFSET), shamt=LOG2_BLOCK_OFFSET] 
                            (const auto& e)
                            {
                                return e.has_value() && e.value().address.slice_upper(LOG2_BLOCK_OFFSET) == address;
                            };

            auto it = std::find_if(WQ.begin(), w_it, checker);
            if (it == w_it)
                it = std::find_if(std::next(w_it), WQ.end(), checker);

            if (w_it != WQ.end())
                w_it->reset();
            else
                w_it->value().forward_checked = true;
        }
    }
}

void
DRAM_CHANNEL::check_read_collision()
{
    for (auto r_it = RQ.begin(); r_it != RQ.end(); r_it++)
    {
        if (r_it->has_value() && !r_it_>value().forward_checked)
        {
            auto address = r_it->value().address;
            auto checker = [match=address.slice_upper(LOG2_BLOCK_OFFSET), shamt=LOG2_BLOCK_OFFSET] 
                            (const auto& e)
                            {
                                return e.has_value() && e.value().address.slice_upper(LOG2_BLOCK_OFFSET) == address;
                            };

            auto it = std::find_if(WQ.begin(), WQ.end(), checker);
            // Forward write:
            if (it != WQ.end())
            {
                response_type response{r_it->value().address,
                                       r_it->value().v_address,
                                       wq_it->value().data,
                                       r_it->value().pf_metadata,
                                       r_it->value().instr_depend_on_me};
                for (auto* ret : r_it->value().to_return)
                    ret->push_back(response);

                r_it->reset();
            }

            // Check for common reads
            auto it = std::find_if(RQ.begin(), r_it, checker);
            if (it == r_it)
                r_it = std::find_if(std::next(r_it), RQ.end(), checker);

            if (it != RQ.end())
            {
                auto instr_copy = std::move(it->value().instr_depend_on_me);
                auto ret_copy = std::move(it->value().to_return);

                std::set_union(std::begin(instr_copy), std::end(instr_copy), std::begin(r_it->value().instr_depend_on_me), std::end(r_it->value().instr_depend_on_me),
                               std::back_inserter(it->value().instr_depend_on_me));
                std::set_union(std::begin(ret_copy), std::end(ret_copy), std::begin(r_it->value().to_return), std::end(r_it->value().to_return),
                               std::back_inserter(it->value().to_return));

                r_it->reset();
            }
            else
            {
                r_it->forward_checked = true;
            }
        }
    }
}

void
DRAM_CHANNEL::update_read_write_priority()
{
    size_t read_occu = std::count_if(RQ.begin(), RQ.end(), [] (const auto& e) { return e.has_value(); });
    size_t write_occu = std::count_if(WQ.begin(), WQ.end(), [] (const auto& e) { return e.has_value(); });

    if (write_mode && (read_occu > 0 && write_occu < low_watermark))
    {
        write_mode = false; 
    }
    else if (!write_mode && ((read_occu == 0 && write_occu > 0) || write_occu >= high_watermark))
    {
        write_mode = true;
    }
}

long
DRAM_CHANNEL::operate()
{
    long progress{0};

    // If we are in warmup, auto-complete requests:
    if (warmup)
    {
        for (auto& entry : RQ)
        {
            if (entry.has_value())
            {
                response_type response{
                    entry->address, entry->v_address, entry->data, entry->pf_metadata, entry->instr_depend_on_me
                };
                for (auto* ret : entry.value().to_return)
                    ret->push_back(response);

                ++progress;
                entry.reset();
            }
        }

        for (auto& entry : WQ)
        {
            if (entry.has_value())
                ++progress;
            entry.reset();
        }

        return progress;
    }

    // Otherwise:
    check_write_collision();
    check_read_collision();
    progress += handle_refresh();
    update_read_write_priority();
    progress += schedule_ready_request();
    progress += complete_requests();

    // Update faw:
    while (!faw.empty() && current_time >= faw.front())
    {
        faw.pop_front();
        ++progress;
    }

    return progress;
}

void
DRAM_CHANNEL::print_deadlock()
{
    std::cerr << "DEADLOCK IN DRAM_CHANNEL -- TODO\n";
}
