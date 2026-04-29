/*
 *  author: Suhas Vittal date:   23 March 2025
 * */

#ifndef DRAM_CHANNEL_h
#define DRAM_CHANNEL_h

#include "address.h"
#include "channel.h"
#include "chrono.h"
#include "dram_address.h"
#include "dram_stats.h"
#include "dram_timing.h"
#include "operable.h"

#include <array>
#include <cstdint>
#include <cstddef>
#include <fstream>
#include <iosfwd>
#include <optional>
#include <utility>
#include <vector>

enum class DRAM_PAGE_POLICY { OPEN =0, CLOSE =1, SOFT_CLOSE =2, OPEN_WRITES_ONLY =3, TIMEOUT_OPEN_PAGE =4 };

struct DRAM_COMMAND
{
    enum class TYPE { INVALID, READ, WRITE, ACTIVATE, PRECHARGE };

    champsim::address address;
    TYPE              type =TYPE::INVALID;

    bool autopre =false;
};

struct DRAM_BANK_STATE
{
    std::optional<std::size_t> open_row{};

    champsim::chrono::clock::time_point act_ok{};
    champsim::chrono::clock::time_point pre_ok{};
    champsim::chrono::clock::time_point read_ok{};
    champsim::chrono::clock::time_point write_ok{};

    // Only used for timeout open page (open for 100ns)
    champsim::chrono::clock::time_point row_open_until{};

    bool next_cas_is_row_hit =false;
};

struct DRAM_CHANNEL final : public champsim::operable
{
    using response_type = typename champsim::channel::response_type;
    struct request_type
    {
        bool scheduled = false;
        bool forward_checked = false;

        uint8_t asid[2] = {std::numeric_limits<uint8_t>::max(), std::numeric_limits<uint8_t>::max()};

        uint32_t pf_metadata = 0;

        champsim::address address{};
        champsim::address v_address{};
        champsim::address data{};
        champsim::chrono::clock::time_point ready_time = champsim::chrono::clock::time_point::max();
        champsim::chrono::clock::time_point install_time{};

        std::vector<uint64_t> instr_depend_on_me{};
        std::vector<std::deque<response_type>*> to_return{};

        explicit request_type(const typename champsim::channel::request_type& req);
    };
    using value_type = request_type;
    using queue_type = std::vector<std::optional<value_type>>;
    using cmd_output_type = std::pair<DRAM_COMMAND, queue_type::iterator>;
    using stats_type = dram_stats;

    struct BANK_DATA
    {
        using active_entry_type = std::pair<bool, queue_type::iterator>;  // is_read, iterator

        DRAM_BANK_STATE                  state{};
        std::optional<active_entry_type> active_request{};
    };
    
    queue_type RQ;
    queue_type WQ;

    bool write_mode =false;
    bool wq_abort=false;

    std::vector<BANK_DATA> banks{};
    std::deque<champsim::chrono::clock::time_point> faw{};
    champsim::chrono::clock::time_point last_ref_cycle{};

    stats_type roi_stats, sim_stats;

    const std::size_t low_watermark;
    const std::size_t high_watermark;

    const size_t num_bankgroups;
    const size_t num_banks;
    const size_t num_rows;

    const size_t channel_id;
    const bool baws;
    const bool bard_wq_abort;
    const bool gwrr;

    std::vector<size_t> wrr_bank_list;
    size_t wrr_list_ptr = 0;
    bool wrr_active = false;
private:
    DRAM_ADDRESS_MAPPER address_mapper;
    DRAM_TIMING         dram_timing;

    size_t bankgroup_conflict = 0; // 6x
    std::vector<bool> bank_scheduled_to;

    bool write_drain_started_with_no_read_occu;
    size_t writes_during_drain =0;
    std::vector<size_t> writes_per_bankgroup;
    std::vector<size_t> writes_per_bank;
    champsim::chrono::clock::time_point write_drain_start{};

    std::ofstream logger{};
    champsim::chrono::clock::time_point last_cas_command_time{};
public:
    DRAM_CHANNEL(champsim::chrono::picoseconds mc_period,
                std::size_t rq_size, std::size_t wq_size, std::size_t low_watermark, std::size_t high_watermark,
                std::size_t channel_id, std::size_t num_bankgroups, std::size_t num_banks, std::size_t num_rows,
                const DRAM_ADDRESS_MAPPER&, const DRAM_TIMING&, bool baws, bool bard_wq_abort, bool gwrr);

    void initialize_wrr_list();

    cmd_output_type find_ready_request(void);
    long schedule_ready_request(void);
    long handle_refresh();
    long complete_requests();

    void check_write_collision();
    void check_read_collision();
    void update_read_write_priority(void);

    void initialize() {}
    void begin_phase() {}
    void end_phase(unsigned) { roi_stats = sim_stats; }

    long operate() final;
    void print_deadlock() final;

    bool does_bank_have_pending_write(size_t) const;
private:
    bool do_autopre(const DRAM_COMMAND&);
};

/*
 * Command-line options:
 * */
extern DRAM_PAGE_POLICY OPT_DRAM_PAGE_POLICY;
extern bool             OPT_DRAM_USE_X8_WRITE_TIMING;

#endif   // DRAM_CHANNEL_h
