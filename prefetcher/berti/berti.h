#ifndef BERTI_H
#define BERTI_H

#include <array>
#include <cstdint>
#include <map>
#include <queue>
#include <vector>
#include <algorithm>
#include <cmath>
#include <cassert>
#include <memory>

#include "address.h"
#include "champsim.h"
#include "modules.h"

// VBerti configuration constants
namespace vberti_config {
    // Table sizes (configurable via preprocessor)
    #ifdef SIZE_2X
        constexpr std::size_t HISTORY_TABLE_SETS = 16;
        constexpr std::size_t HISTORY_TABLE_WAYS = 32;
        constexpr std::size_t BERTI_TABLE_SIZE = 32;
        constexpr std::size_t BERTI_STRIDE_SIZE = 32;
        constexpr uint64_t TABLE_SET_MASK = 0xF;
    #elif defined(SIZE_4X)
        constexpr std::size_t HISTORY_TABLE_SETS = 32;
        constexpr std::size_t HISTORY_TABLE_WAYS = 64;
        constexpr std::size_t BERTI_TABLE_SIZE = 64;
        constexpr std::size_t BERTI_STRIDE_SIZE = 64;
        constexpr uint64_t TABLE_SET_MASK = 0x1F;
    #elif defined(SIZE_050X)
        constexpr std::size_t HISTORY_TABLE_SETS = 4;
        constexpr std::size_t HISTORY_TABLE_WAYS = 8;
        constexpr std::size_t BERTI_TABLE_SIZE = 8;
        constexpr std::size_t BERTI_STRIDE_SIZE = 8;
        constexpr uint64_t TABLE_SET_MASK = 0x3;
    #elif defined(SIZE_025X)
        constexpr std::size_t HISTORY_TABLE_SETS = 2;
        constexpr std::size_t HISTORY_TABLE_WAYS = 4;
        constexpr std::size_t BERTI_TABLE_SIZE = 4;
        constexpr std::size_t BERTI_STRIDE_SIZE = 4;
        constexpr uint64_t TABLE_SET_MASK = 0x1;
    #else // SIZE_1X (default)
        constexpr std::size_t HISTORY_TABLE_SETS = 8;
        constexpr std::size_t HISTORY_TABLE_WAYS = 16;
        constexpr std::size_t BERTI_TABLE_SIZE = 16;
        constexpr std::size_t BERTI_STRIDE_SIZE = 16;
        constexpr uint64_t TABLE_SET_MASK = 0x7;
    #endif

    // Algorithm parameters
    constexpr std::size_t LATENCY_TABLE_SIZE = 32; // L1D_MSHR_SIZE + 16
    constexpr std::size_t MAX_HISTORY_IP = 8;
    constexpr std::size_t MAX_PREFETCH = 16;
    constexpr std::size_t MAX_PREFETCH_LAUNCH = 12;
    constexpr std::size_t STRIDE_MASK_BITS = 12;

    // Bit masks
    constexpr uint64_t IP_MASK = 0x3FF;
    constexpr uint64_t TIME_MASK = 0xFFFF;
    constexpr uint64_t LATENCY_MASK = 0xFFF;
    constexpr uint64_t ADDRESS_MASK = 0xFFFFFF;

    // Confidence thresholds
    constexpr uint64_t CONFIDENCE_MAX = 16;
    constexpr uint64_t CONFIDENCE_INCREMENT = 1;
    constexpr uint64_t CONFIDENCE_INIT = 1;
    constexpr uint64_t CONFIDENCE_L1_THRESHOLD = 65;
    constexpr uint64_t CONFIDENCE_L2_THRESHOLD = 50;
    constexpr uint64_t CONFIDENCE_L2R_THRESHOLD = 35;
    constexpr float MSHR_LIMIT_PERCENT = 70.0f;
    constexpr uint64_t LAUNCH_THRESHOLD = 8;
}

struct berti : public champsim::modules::prefetcher {
public:
    // Modern type aliases (using instead of typedef)
    using address_type = champsim::address;
    using block_number_type = champsim::block_number;

    // Replacement policy levels
    enum class replacement_level : uint8_t {
        NOT_REPLACEABLE = 0,
        L1_LEVEL = 1,
        L2_LEVEL = 2,
        L2_REPLACEABLE = 3
    };

    // Data structures
    struct latency_entry_type {
        uint64_t address{0};
        uint64_t tag{0};
        uint64_t timestamp{0};
        bool is_prefetch{false};

        latency_entry_type() = default;
        latency_entry_type(uint64_t addr, uint64_t t, uint64_t ts, bool pf)
            : address(addr), tag(t), timestamp(ts), is_prefetch(pf) {}
    };

    struct history_entry_type {
        uint64_t tag{0};
        uint64_t address{0};
        uint64_t timestamp{0};

        history_entry_type() = default;
        history_entry_type(uint64_t t, uint64_t addr, uint64_t ts)
            : tag(t), address(addr), timestamp(ts) {}
    };

    struct stride_entry_type {
        uint64_t confidence{0};
        int64_t stride{0};
        replacement_level level{replacement_level::NOT_REPLACEABLE};
        float percentage{0.0f};

        stride_entry_type() = default;
        stride_entry_type(int64_t s, uint64_t conf = vberti_config::CONFIDENCE_INIT)
            : confidence(conf), stride(s), level(replacement_level::NOT_REPLACEABLE) {}
    };

    struct berti_entry_type {
        std::array<stride_entry_type, vberti_config::BERTI_STRIDE_SIZE> strides;
        uint64_t confidence{0};
        uint64_t total_used{0};

        berti_entry_type() = default;
    };

    struct shadow_cache_entry_type {
        uint64_t address{0};
        uint64_t latency{0};
        bool is_prefetch{false};

        shadow_cache_entry_type() = default;
        shadow_cache_entry_type(uint64_t addr, uint64_t lat, bool pf)
            : address(addr), latency(lat), is_prefetch(pf) {}
    };

private:
    // Internal data structures (no more CPU arrays!)
    std::array<latency_entry_type, vberti_config::LATENCY_TABLE_SIZE> latency_table;

    std::array<std::array<history_entry_type, vberti_config::HISTORY_TABLE_WAYS>,
               vberti_config::HISTORY_TABLE_SETS> history_table;

    std::array<std::size_t, vberti_config::HISTORY_TABLE_SETS> history_pointers;

    std::array<std::array<shadow_cache_entry_type, 8>, 64> shadow_cache; // L1D_WAY, L1D_SET

    std::map<uint64_t, std::unique_ptr<berti_entry_type>> berti_table;
    std::queue<uint64_t> berti_fifo_queue;

    // Helper methods
    void initialize_latency_table();
    void initialize_history_table();
    void initialize_shadow_cache();

    bool add_to_latency_table(uint64_t line_addr, uint64_t tag, bool is_prefetch, uint64_t cycle);
    uint64_t remove_from_latency_table(uint64_t line_addr);
    uint64_t get_latency_table_tag(uint64_t line_addr) const;
    uint64_t get_latency_table_timestamp(uint64_t line_addr) const;

    void add_to_history_table(uint64_t tag, uint64_t address);
    std::size_t get_history_entries(uint64_t tag, uint64_t current_addr, uint64_t latency,
                                   uint64_t cycle, std::vector<uint64_t>& ips,
                                   std::vector<uint64_t>& addresses) const;

    void add_to_shadow_cache(std::size_t set, std::size_t way, uint64_t line_addr,
                            bool is_prefetch, uint64_t latency);
    bool is_in_shadow_cache(uint64_t line_addr) const;
    bool mark_shadow_cache_accessed(uint64_t line_addr);
    bool is_shadow_cache_prefetch(uint64_t line_addr) const;
    uint64_t get_shadow_cache_latency(uint64_t line_addr) const;

    void add_to_berti_table(uint64_t tag, int64_t stride);
    void increase_berti_confidence(uint64_t tag);
    bool get_berti_strides(uint64_t tag, std::vector<stride_entry_type>& strides) const;

    void find_and_update_patterns(uint64_t latency, uint64_t tag, uint64_t cycle, uint64_t line_addr);

    static bool compare_strides_by_priority(const stride_entry_type& a, const stride_entry_type& b);
    static bool compare_strides_by_percentage(const stride_entry_type& a, const stride_entry_type& b);

public:
    using champsim::modules::prefetcher::prefetcher;

    void prefetcher_initialize();
    uint32_t prefetcher_cache_operate(address_type addr, address_type ip, uint8_t cache_hit,
                                     bool useful_prefetch, access_type type, uint32_t metadata_in);
    uint32_t prefetcher_cache_fill(address_type addr, long set, long way, uint8_t prefetch,
                                  address_type evicted_addr, uint32_t metadata_in);
    void prefetcher_cycle_operate();
    void prefetcher_final_stats();
};

#endif // BERTI_H
