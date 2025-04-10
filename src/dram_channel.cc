/*
 *  author: Suhas Vittal
 *  date:   23 March 2025
 * */

#include "dram_channel.h"

#include <cmath>
#include <iostream>

DRAM_CHANNEL::request_type::request_type(const typename champsim::channel::request_type& req)
    : pf_metadata(req.pf_metadata), address(req.address), v_address(req.address), data(req.data), instr_depend_on_me(req.instr_depend_on_me)
{
  asid[0] = req.asid[0];
  asid[1] = req.asid[1];
}

//#define DRAM_ENABLE_LOGGER

DRAM_CHANNEL::DRAM_CHANNEL(
        champsim::chrono::picoseconds mc_period,
        std::size_t rq_size,
        std::size_t wq_size,
        std::size_t _channel_id,
        std::size_t _num_bankgroups,
        std::size_t _num_banks,
        std::size_t _num_rows,
        const DRAM_ADDRESS_MAPPER& am,
        const DRAM_TIMING& timing)
    :champsim::operable(mc_period),
    RQ(rq_size),
    WQ(wq_size),
    banks(_num_bankgroups*_num_banks, BANK_DATA{}),
    low_watermark(wq_size/6),
    high_watermark((5*wq_size)/6),
    num_bankgroups(_num_bankgroups),
    num_banks(_num_banks),
    num_rows(_num_rows),
    channel_id(_channel_id),
    address_mapper(am),
    dram_timing(timing),
    writes_per_bank(_num_bankgroups*_num_banks, 0),
    writes_per_bankgroup(_num_bankgroups, 0)
{
#if defined(DRAM_ENABLE_LOGGER)
    logger = std::ofstream("dram_channel." + std::to_string(channel_id) + ".log");
#endif
}

DRAM_CHANNEL::cmd_output_type
DRAM_CHANNEL::find_ready_request()
{
    cmd_output_type out;

    // First check active buffer for any issuable commands:
    for (const auto& b : banks)
    {
        if (!b.active_request.has_value())
            continue;

        // Check if the request is issuable:
        auto& [is_read, req_it] = b.active_request.value();
        if ((is_read && current_time >= b.state.read_ok) || (!is_read && current_time >= b.state.write_ok))
        {
            DRAM_COMMAND::TYPE cmd_type = is_read ? DRAM_COMMAND::TYPE::READ : DRAM_COMMAND::TYPE::WRITE;
            DRAM_COMMAND ready_cmd{req_it->value().address, cmd_type};
            ready_cmd.autopre = do_autopre(ready_cmd);

            if (out.first.type == DRAM_COMMAND::TYPE::INVALID
                || (out.second->value().ready_time > req_it->value().ready_time))
            {
                out = cmd_output_type{ready_cmd, req_it};
            }
        }
    }

    if (out.first.type != DRAM_COMMAND::TYPE::INVALID)
        return out;

    // If nothing could be done, schedule reads and writes:
    auto& q = write_mode ? WQ : RQ;

    std::vector<bool> banks_with_row_hits(num_bankgroups*num_banks, false);
    for (auto it = q.begin(); it != q.end(); it++)
    {
        if (!it->has_value())
            continue;

        const auto& req = it->value();
        if (req.scheduled)
            continue;

        size_t b_idx = address_mapper.bank_idx(req.address);
        const auto& b = banks.at(b_idx);

        if (b.state.open_row.has_value() && b.state.open_row.value() == address_mapper.row(req.address))
            banks_with_row_hits[b_idx] = true;
    }

    for (auto it = q.begin(); it != q.end(); it++)
    {
        if (!it->has_value())
            continue;

        const auto& req = it->value();
        if (req.scheduled)
            continue;

        size_t b_idx = address_mapper.bank_idx(req.address);
        const auto& b = banks.at(b_idx);

        DRAM_COMMAND ready_cmd{};
        ready_cmd.address = req.address;
        
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
            else if (!banks_with_row_hits[b_idx] && current_time >= b.state.pre_ok)
            {
                ready_cmd.type = DRAM_COMMAND::TYPE::PRECHARGE;
            }
        }
        else if (current_time >= b.state.act_ok && faw.size() < 4)
        {
            ready_cmd.type = DRAM_COMMAND::TYPE::ACTIVATE;
        }

        if (ready_cmd.type != DRAM_COMMAND::TYPE::INVALID &&
            (out.first.type == DRAM_COMMAND::TYPE::INVALID || req.ready_time < out.second->value().ready_time))
        {
            if (ready_cmd.type == DRAM_COMMAND::TYPE::READ || ready_cmd.type == DRAM_COMMAND::TYPE::WRITE)
                ready_cmd.autopre = do_autopre(ready_cmd);

            out = cmd_output_type{ready_cmd, it};
        }
    }

    return out;
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

    auto update = [this] (champsim::chrono::clock::time_point& t, champsim::chrono::clock::duration delta)
    {
        t = std::max(t, this->current_time + delta);
    };

    [[ maybe_unused ]] uint64_t latency = (current_time - q_it->value().install_time).count() / 1000;
    if (cmd.type == DRAM_COMMAND::TYPE::READ || cmd.type == DRAM_COMMAND::TYPE::WRITE)
    {
        const bool is_read = cmd.type == DRAM_COMMAND::TYPE::READ;
            
        q_it->value().scheduled = true;
        q_it->value().ready_time = current_time + (is_read ? (dram_timing.CL + dram_timing.burst) 
                                                           : (dram_timing.CWL + dram_timing.burst));

        if (is_read)
        {
            ++sim_stats.reads;
            sim_stats.read_row_hits += b.state.next_cas_is_row_hit;
            sim_stats.tot_read_latency += (q_it->value().ready_time - q_it->value().install_time).count() / 1000;
        }
        else
        {
            ++sim_stats.writes;
            sim_stats.write_row_hits += b.state.next_cas_is_row_hit;
            ++writes_during_drain;

            ++writes_per_bankgroup[bg];
            ++writes_per_bank[b_idx];
        }

        // Sanity check:
        if (!b.state.open_row.has_value() || b.state.open_row.value() != address_mapper.row(cmd.address))
        {
            std::cerr << "Received invalid column command\n";
            exit(1);
        }

        b.active_request.reset();

        if (cmd.autopre)
        {
            update(b.state.act_ok, is_read ? dram_timing.tRTP + dram_timing.tRP
                                           : (dram_timing.CWL + dram_timing.burst + dram_timing.tWR + dram_timing.tRP));
            b.state.open_row.reset();
            b.state.next_cas_is_row_hit = false;

            ++sim_stats.precharges;
        }
        else
        {
            update(b.state.pre_ok, is_read ? dram_timing.tRTP 
                                           : (dram_timing.CWL + dram_timing.burst + dram_timing.tWR));
            b.state.next_cas_is_row_hit = true;
        }
    }
    else if (cmd.type == DRAM_COMMAND::TYPE::ACTIVATE)
    {
        // Sanity check:
        if (b.state.open_row.has_value())
        {
            std::cerr << "Received activate when row is open\n";
            exit(1);
        }

        // Update bank state and active entry;
        b.active_request = BANK_DATA::active_entry_type{!write_mode, q_it};
        b.state.open_row = address_mapper.row(cmd.address);

        update(b.state.read_ok, dram_timing.tRCD);
        update(b.state.write_ok, dram_timing.tRCD);
        update(b.state.pre_ok, dram_timing.tRAS);

        // Update faw:
        faw.push_back(current_time + dram_timing.tFAW);

        ++sim_stats.activates;
    }
    else if (cmd.type == DRAM_COMMAND::TYPE::PRECHARGE)
    {
        if (!b.state.open_row.has_value())
        {
            std::cerr << "Received precharge when row is closed\n";
            exit(1);
        }

        b.state.open_row.reset();
        b.state.next_cas_is_row_hit = false;
        update(b.state.act_ok, dram_timing.tRP);

        ++sim_stats.precharges;
    }
    else
    {
        std::cerr << "unknown dram command\n";
        exit(1);
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
                update(bb.state.read_ok, same_bankgroup ? dram_timing.tCCD_L : dram_timing.tCCD_S);
                update(bb.state.write_ok, dram_timing.tCCD_RTW);
            }
            else if (cmd.type == DRAM_COMMAND::TYPE::WRITE)
            {
                update(bb.state.read_ok, same_bankgroup ? dram_timing.tCCD_L_WTR : dram_timing.tCCD_S_WTR);
                if (opt_dram_use_x8_write_timing)
                    update(bb.state.write_ok, same_bankgroup ? dram_timing.tCCD_L_WR/2 : dram_timing.tCCD_S_WR);
                else
                    update(bb.state.write_ok, same_bankgroup ? dram_timing.tCCD_L_WR : dram_timing.tCCD_S_WR);
            }
            else if (cmd.type == DRAM_COMMAND::TYPE::ACTIVATE)
            {
                update(bb.state.act_ok, same_bankgroup ? dram_timing.tRRD_L : dram_timing.tRRD_S);
            }
            // No bankgroup timings for precharge
            ++ii;
        }
    }

#if defined(DRAM_ENABLE_LOGGER)
    std::string cmd_string;
    
    if (cmd.type == DRAM_COMMAND::TYPE::READ)
        cmd_string = "READ";
    else if (cmd.type == DRAM_COMMAND::TYPE::WRITE)
        cmd_string = "WRITE";
    else if (cmd.type == DRAM_COMMAND::TYPE::PRECHARGE)
        cmd_string = "PRECHARGE";
    else
        cmd_string = "ACTIVATE";

    size_t row = address_mapper.row(cmd.address);
    uint64_t delta = (current_time - last_cas_command_time).count() / 1000;

    logger << cmd_string << "\tbank = " << b_idx << "\trow = " << row << "\t+" << delta << " ns";

    if (cmd.type == DRAM_COMMAND::TYPE::READ || cmd.type == DRAM_COMMAND::TYPE::WRITE)
        logger << "\t(T = " << latency << ")";

    logger << "\n";

    last_cas_command_time = current_time;
#endif

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
            response_type response{it->value().address,
                                    it->value().v_address,
                                    it->value().data,
                                    it->value().pf_metadata,
                                    it->value().instr_depend_on_me};

            for (auto* ret : it->value().to_return)
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
            champsim::data::bits offset{LOG2_BLOCK_SIZE};
            auto checker = [match=address.slice_upper(offset), shamt=offset] 
                            (const auto& e)
                            {
                                return e.has_value() && (e.value().address.slice_upper(shamt) == match);
                            };

            auto found = std::find_if(WQ.begin(), w_it, checker);
            if (found == w_it)
                found = std::find_if(std::next(w_it), WQ.end(), checker);

            if (found != WQ.end())
            {
                w_it->reset();
            }
            else
            {
                w_it->value().forward_checked = true;
                
                // Set bank id directly:
                if (opt_dram_ideal_wlp)
                {
                    w_it->value().address = address_mapper.set_bank_idx_of_address(w_it->value().address, wlp_bank_ctr);

                    ++wlp_bank_ctr;
                    if (wlp_bank_ctr == num_bankgroups*num_banks)
                        wlp_bank_ctr = 0;
                }
            }
        }
    }
}

void
DRAM_CHANNEL::check_read_collision()
{
    for (auto r_it = RQ.begin(); r_it != RQ.end(); r_it++)
    {
        if (r_it->has_value() && !r_it->value().forward_checked)
        {
            auto address = r_it->value().address;
            champsim::data::bits offset{LOG2_BLOCK_SIZE};
            auto checker = [match=address.slice_upper(offset), shamt=offset] 
                            (const auto& e)
                            {
                                return e.has_value() && e.value().address.slice_upper(shamt) == match;
                            };

            auto found = std::find_if(WQ.begin(), WQ.end(), checker);
            // Forward write:
            if (found != WQ.end())
            {
                response_type response{r_it->value().address,
                                       r_it->value().v_address,
                                       found->value().data,
                                       r_it->value().pf_metadata,
                                       r_it->value().instr_depend_on_me};
                for (auto* ret : r_it->value().to_return)
                    ret->push_back(response);

                r_it->reset();
                continue;
            }

            // Check for common reads
            found = std::find_if(RQ.begin(), r_it, checker);
            if (found == r_it)
                found = std::find_if(std::next(r_it), RQ.end(), checker);

            if (found != RQ.end())
            {
                auto instr_copy = std::move(found->value().instr_depend_on_me);
                auto ret_copy = std::move(found->value().to_return);

                std::set_union(std::begin(instr_copy), std::end(instr_copy), std::begin(r_it->value().instr_depend_on_me), std::end(r_it->value().instr_depend_on_me),
                               std::back_inserter(found->value().instr_depend_on_me));
                std::set_union(std::begin(ret_copy), std::end(ret_copy), std::begin(r_it->value().to_return), std::end(r_it->value().to_return),
                               std::back_inserter(found->value().to_return));

                r_it->reset();
            }
            else
            {
                r_it->value().forward_checked = true;
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

        uint64_t write_time = (current_time - write_drain_start).count();
        sim_stats.tot_time_in_write_mode += write_time;

        if (writes_during_drain)
        {
            ++sim_stats.num_write_drains;
            if (!write_drain_started_with_no_read_occu)
                ++sim_stats.num_forced_write_drains;

            auto [min_it, max_it] = std::minmax_element(writes_per_bank.begin(), writes_per_bank.end());
            sim_stats.tot_write_imbalance += *max_it - *min_it;

            sim_stats.tot_read_occu_post_drain += read_occu;
            
            size_t banks_with_writes = std::count_if(writes_per_bank.begin(), writes_per_bank.end(), [] (auto x) { return x != 0; });
            sim_stats.tot_bank_parallelism += banks_with_writes;

            size_t bankgroups_with_writes = std::count_if(writes_per_bankgroup.begin(), writes_per_bankgroup.end(), [] (auto x) { return x != 0; });
            sim_stats.tot_bankgroup_parallelism += bankgroups_with_writes;
        }
    }
    else if (!write_mode && ((read_occu == 0 && write_occu > 0) || write_occu >= high_watermark))
    {
        write_mode = true;

        writes_during_drain = 0;
        write_drain_start = current_time;
        write_drain_started_with_no_read_occu = (read_occu == 0);

        sim_stats.tot_read_occu_pre_drain += read_occu;

        std::fill(writes_per_bank.begin(), writes_per_bank.end(), 0);
        std::fill(writes_per_bankgroup.begin(), writes_per_bankgroup.end(), 0);
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
    progress += complete_requests();
    check_write_collision();
    check_read_collision();
    progress += handle_refresh();
    update_read_write_priority();
    progress += schedule_ready_request();

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
    fmt::print("CHANNEL {} DEADLOCK:\n", channel_id);

    size_t read_occu = std::count_if(RQ.begin(), RQ.end(), [] (const auto& e) { return e.has_value(); });
    size_t write_occu = std::count_if(WQ.begin(), WQ.end(), [] (const auto& e) { return e.has_value(); });

    fmt::print("\tREAD OCCU = {}, WRITE OCCU = {}\n", read_occu, write_occu);
}

bool
DRAM_CHANNEL::do_autopre(const DRAM_COMMAND& cmd)
{
    if (opt_dram_page_policy == DRAM_PAGE_POLICY::OPEN)
    {
        return false;
    }
    else if (opt_dram_page_policy == DRAM_PAGE_POLICY::CLOSE)
    {
        return true;
    }
    else if (opt_dram_page_policy == DRAM_PAGE_POLICY::SOFT_CLOSE)
    {
        auto& q = write_mode ? WQ : RQ;

        size_t b_idx = address_mapper.bank_idx(cmd.address),
               row = address_mapper.row(cmd.address);

        bool no_row_hits = std::none_of(q.begin(), q.end(),
                                [this, b_idx, row, address=cmd.address] (const auto& e) 
                                {
                                    return e.has_value()
                                        && !e.value().scheduled
                                        && e.value().address != address
                                        && this->address_mapper.bank_idx(e.value().address) == b_idx
                                        && this->address_mapper.row(e.value().address) == row;
                                });
        return no_row_hits;
    }
    else if (opt_dram_page_policy == DRAM_PAGE_POLICY::OPEN_WRITES_ONLY)
    {
        if (cmd.type == DRAM_COMMAND::TYPE::WRITE)
            return false;
        else
            return true;
    }
}
