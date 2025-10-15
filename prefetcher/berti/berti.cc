#include "berti.h"
#include "cache.h"
#include <iostream>

void berti::prefetcher_initialize()
{
    initialize_latency_table();
    initialize_history_table();
    initialize_shadow_cache();
    
    std::cout << "VBerti Prefetcher Initialized:" << std::endl;
    std::cout << "  History Sets: " << vberti_config::HISTORY_TABLE_SETS << std::endl;
    std::cout << "  History Ways: " << vberti_config::HISTORY_TABLE_WAYS << std::endl;
    std::cout << "  Berti Table Size: " << vberti_config::BERTI_TABLE_SIZE << std::endl;
    std::cout << "  Berti Stride Size: " << vberti_config::BERTI_STRIDE_SIZE << std::endl;
}

uint32_t berti::prefetcher_cache_operate(address_type addr, address_type ip, uint8_t cache_hit,
                                        bool useful_prefetch, access_type type, uint32_t metadata_in)
{
    // Only handle LOAD and RFO (store) operations
    if (type != access_type::LOAD && type != access_type::RFO) {
        return metadata_in;
    }
    
    uint64_t line_addr = addr.to<uint64_t>() >> LOG2_BLOCK_SIZE;
    
    // Hash the IP for better distribution
    uint64_t hashed_ip = ((ip.to<uint64_t>() >> 1) ^ (ip.to<uint64_t>() >> 4)) & vberti_config::IP_MASK;
    
    if (!cache_hit) {
        // Cache miss - add to latency table and history table
        add_to_latency_table(line_addr, hashed_ip, false, intern_->current_cycle());
        add_to_history_table(hashed_ip, line_addr);
    } 
    else if (cache_hit && is_shadow_cache_prefetch(line_addr)) {
        // Cache hit on a prefetched line - update patterns
        mark_shadow_cache_accessed(line_addr);
        uint64_t latency = get_shadow_cache_latency(line_addr);
        find_and_update_patterns(latency, hashed_ip, intern_->current_cycle(), line_addr);
        add_to_history_table(hashed_ip, line_addr);
    } 
    else {
        // Regular cache hit - just mark as accessed
        mark_shadow_cache_accessed(line_addr);
    }
    
    // Generate prefetches based on learned patterns
    std::vector<stride_entry_type> strides;
    if (get_berti_strides(hashed_ip, strides)) {
        int launched = 0;
        float mshr_load = static_cast<float>(intern_->get_mshr_occupancy()) / 
                         static_cast<float>(intern_->get_mshr_size()) * 100.0f;
        
        for (std::size_t i = 0; i < vberti_config::MAX_PREFETCH_LAUNCH && i < strides.size(); ++i) {
            if (strides[i].level == replacement_level::NOT_REPLACEABLE) {
                continue;
            }
            
            uint64_t prefetch_addr = (line_addr + strides[i].stride) << LOG2_BLOCK_SIZE;
            uint64_t prefetch_block_addr = prefetch_addr >> LOG2_BLOCK_SIZE;
            
            // Check if already being tracked or in shadow cache
            if (get_latency_table_timestamp(prefetch_block_addr) != 0) {
                continue;
            }
            
            // Determine fill level based on confidence
            bool fill_this_level = false;
            if (strides[i].level == replacement_level::L1_LEVEL && 
                mshr_load < vberti_config::MSHR_LIMIT_PERCENT) {
                fill_this_level = true;
            } else if (strides[i].level == replacement_level::L2_LEVEL || 
                      strides[i].level == replacement_level::L2_REPLACEABLE) {
                fill_this_level = false; // Prefetch to L2
            } else {
                continue;
            }
            
            // Issue prefetch
            if (prefetch_line(address_type{prefetch_addr}, fill_this_level, 0)) {
                launched++;
            }
        }
    }
    
    return metadata_in;
}

uint32_t berti::prefetcher_cache_fill(address_type addr, long set, long way, uint8_t prefetch, 
                                      address_type evicted_addr, uint32_t metadata_in)
{
    uint64_t line_addr = addr.to<uint64_t>() >> LOG2_BLOCK_SIZE;
    
    // Remove from latency table and get timing information
    uint64_t tag = get_latency_table_tag(line_addr);
    uint64_t cycle = get_latency_table_timestamp(line_addr);
    uint64_t latency = remove_from_latency_table(line_addr);
    
    if (latency > vberti_config::LATENCY_MASK) {
        latency = 0;
    }
    
    // Add to shadow cache
    add_to_shadow_cache(static_cast<std::size_t>(set), static_cast<std::size_t>(way), 
                       line_addr, prefetch != 0, latency);
    
    // Update patterns for demand misses
    if (latency != 0 && !prefetch) {
        find_and_update_patterns(latency, tag, cycle, line_addr);
    }
    
    return metadata_in;
}

void berti::prefetcher_cycle_operate()
{
    // VBerti doesn't need cycle-level operations in the current implementation
    // All work is done in cache_operate and cache_fill
}

void berti::prefetcher_final_stats()
{
    std::cout << "VBerti Prefetcher Final Statistics:" << std::endl;
    std::cout << "  Berti table entries: " << berti_table.size() << std::endl;
}

// Helper method implementations
void berti::initialize_latency_table()
{
    for (auto& entry : latency_table) {
        entry = latency_entry_type{};
    }
}

void berti::initialize_history_table()
{
    for (std::size_t i = 0; i < vberti_config::HISTORY_TABLE_SETS; ++i) {
        history_pointers[i] = 0;
        for (std::size_t j = 0; j < vberti_config::HISTORY_TABLE_WAYS; ++j) {
            history_table[i][j] = history_entry_type{};
        }
    }
}

void berti::initialize_shadow_cache()
{
    for (auto& set : shadow_cache) {
        for (auto& way : set) {
            way = shadow_cache_entry_type{};
        }
    }
}

bool berti::add_to_latency_table(uint64_t line_addr, uint64_t tag, bool is_prefetch, uint64_t cycle)
{
    uint64_t masked_cycle = cycle & vberti_config::TIME_MASK;
    
    // Check if entry already exists
    for (auto& entry : latency_table) {
        if (entry.address == line_addr) {
            entry.timestamp = masked_cycle;
            entry.tag = tag;
            entry.is_prefetch = is_prefetch;
            return entry.is_prefetch;
        }
    }
    
    // Find free entry
    for (auto& entry : latency_table) {
        if (entry.tag == 0) {
            entry.address = line_addr;
            entry.timestamp = masked_cycle;
            entry.tag = tag;
            entry.is_prefetch = is_prefetch;
            return entry.is_prefetch;
        }
    }
    
    return false; // No free space
}

uint64_t berti::remove_from_latency_table(uint64_t line_addr)
{
    for (auto& entry : latency_table) {
        if (entry.address == line_addr) {
            uint64_t latency = (intern_->current_cycle() & vberti_config::TIME_MASK) - entry.timestamp;
            entry = latency_entry_type{}; // Clear entry
            return latency;
        }
    }
    return 0;
}

uint64_t berti::get_latency_table_tag(uint64_t line_addr) const
{
    for (const auto& entry : latency_table) {
        if (entry.address == line_addr && entry.tag != 0) {
            return entry.tag;
        }
    }
    return 0;
}

uint64_t berti::get_latency_table_timestamp(uint64_t line_addr) const
{
    for (const auto& entry : latency_table) {
        if (entry.address == line_addr) {
            return entry.timestamp;
        }
    }
    return 0;
}

void berti::add_to_history_table(uint64_t tag, uint64_t address)
{
    std::size_t set = tag & vberti_config::TABLE_SET_MASK;
    uint64_t masked_addr = address & vberti_config::ADDRESS_MASK;
    uint64_t cycle = intern_->current_cycle() & vberti_config::TIME_MASK;

    // Add to current pointer position
    std::size_t& pointer = history_pointers[set];
    history_table[set][pointer] = history_entry_type{tag, masked_addr, cycle};

    // Update pointer (circular buffer)
    pointer = (pointer + 1) % vberti_config::HISTORY_TABLE_WAYS;
}

std::size_t berti::get_history_entries(uint64_t tag, uint64_t current_addr, uint64_t latency,
                                       uint64_t cycle, std::vector<uint64_t>& ips,
                                       std::vector<uint64_t>& addresses) const
{
    std::size_t set = tag & vberti_config::TABLE_SET_MASK;
    std::size_t num_found = 0;

    if (cycle < latency) return 0;
    uint64_t target_cycle = cycle - latency;

    // Search through history table in reverse order (most recent first)
    std::size_t pointer = history_pointers[set];
    for (std::size_t i = 0; i < vberti_config::HISTORY_TABLE_WAYS; ++i) {
        std::size_t idx = (pointer + vberti_config::HISTORY_TABLE_WAYS - 1 - i) % vberti_config::HISTORY_TABLE_WAYS;
        const auto& entry = history_table[set][idx];

        if (entry.tag == tag && entry.timestamp <= target_cycle) {
            // Avoid duplicates
            if (entry.address == current_addr) continue;

            bool found_duplicate = false;
            for (std::size_t j = 0; j < num_found; ++j) {
                if (addresses[j] == entry.address) {
                    found_duplicate = true;
                    break;
                }
            }

            if (!found_duplicate && num_found < vberti_config::MAX_HISTORY_IP) {
                ips[num_found] = entry.tag;
                addresses[num_found] = entry.address;
                num_found++;
            }
        }
    }

    return num_found;
}

void berti::add_to_shadow_cache(std::size_t set, std::size_t way, uint64_t line_addr,
                                bool is_prefetch, uint64_t latency)
{
    if (set < shadow_cache.size() && way < shadow_cache[set].size()) {
        shadow_cache[set][way] = shadow_cache_entry_type{line_addr, latency, is_prefetch};
    }
}

bool berti::is_in_shadow_cache(uint64_t line_addr) const
{
    for (const auto& set : shadow_cache) {
        for (const auto& way : set) {
            if (way.address == line_addr) {
                return true;
            }
        }
    }
    return false;
}

bool berti::mark_shadow_cache_accessed(uint64_t line_addr)
{
    for (auto& set : shadow_cache) {
        for (auto& way : set) {
            if (way.address == line_addr) {
                way.is_prefetch = false;
                return true;
            }
        }
    }
    return false;
}

bool berti::is_shadow_cache_prefetch(uint64_t line_addr) const
{
    for (const auto& set : shadow_cache) {
        for (const auto& way : set) {
            if (way.address == line_addr) {
                return way.is_prefetch;
            }
        }
    }
    return false;
}

uint64_t berti::get_shadow_cache_latency(uint64_t line_addr) const
{
    for (const auto& set : shadow_cache) {
        for (const auto& way : set) {
            if (way.address == line_addr) {
                return way.latency;
            }
        }
    }
    return 0;
}

void berti::add_to_berti_table(uint64_t tag, int64_t stride)
{
    auto it = berti_table.find(tag);

    if (it == berti_table.end()) {
        // Create new entry
        if (berti_fifo_queue.size() >= vberti_config::BERTI_TABLE_SIZE) {
            uint64_t old_tag = berti_fifo_queue.front();
            berti_table.erase(old_tag);
            berti_fifo_queue.pop();
        }

        berti_fifo_queue.push(tag);
        auto new_entry = std::make_unique<berti_entry_type>();
        new_entry->confidence = vberti_config::CONFIDENCE_INCREMENT;
        new_entry->strides[0] = stride_entry_type{stride, vberti_config::CONFIDENCE_INIT};

        berti_table[tag] = std::move(new_entry);
        return;
    }

    // Update existing entry
    auto& entry = it->second;

    // Look for existing stride
    for (auto& stride_entry : entry->strides) {
        if (stride_entry.stride == stride) {
            stride_entry.confidence += vberti_config::CONFIDENCE_INCREMENT;
            if (stride_entry.confidence > vberti_config::CONFIDENCE_MAX) {
                stride_entry.confidence = vberti_config::CONFIDENCE_MAX;
            }
            return;
        }
    }

    // Find slot for new stride (prefer NOT_REPLACEABLE slots)
    int best_slot = -1;
    uint64_t min_confidence = vberti_config::CONFIDENCE_MAX + 1;

    for (std::size_t i = 0; i < vberti_config::BERTI_STRIDE_SIZE; ++i) {
        if (entry->strides[i].level == replacement_level::NOT_REPLACEABLE &&
            entry->strides[i].confidence < min_confidence) {
            min_confidence = entry->strides[i].confidence;
            best_slot = static_cast<int>(i);
        }
    }

    // If no NOT_REPLACEABLE slot, try L2_REPLACEABLE
    if (best_slot == -1) {
        min_confidence = vberti_config::CONFIDENCE_MAX + 1;
        for (std::size_t i = 0; i < vberti_config::BERTI_STRIDE_SIZE; ++i) {
            if (entry->strides[i].level == replacement_level::L2_REPLACEABLE &&
                entry->strides[i].confidence < min_confidence) {
                min_confidence = entry->strides[i].confidence;
                best_slot = static_cast<int>(i);
            }
        }
    }

    if (best_slot >= 0) {
        entry->strides[best_slot] = stride_entry_type{stride, vberti_config::CONFIDENCE_INIT};
    }
}

void berti::increase_berti_confidence(uint64_t tag)
{
    auto it = berti_table.find(tag);
    if (it == berti_table.end()) return;

    auto& entry = it->second;
    entry->confidence += vberti_config::CONFIDENCE_INCREMENT;

    if (entry->confidence >= vberti_config::CONFIDENCE_MAX) {
        // Update stride levels based on confidence percentages
        for (auto& stride_entry : entry->strides) {
            float percentage = static_cast<float>(stride_entry.confidence) /
                              static_cast<float>(entry->confidence) * 100.0f;

            if (percentage > vberti_config::CONFIDENCE_L1_THRESHOLD) {
                stride_entry.level = replacement_level::L1_LEVEL;
            } else if (percentage > vberti_config::CONFIDENCE_L2_THRESHOLD) {
                stride_entry.level = replacement_level::L2_LEVEL;
            } else if (percentage > vberti_config::CONFIDENCE_L2R_THRESHOLD) {
                stride_entry.level = replacement_level::L2_REPLACEABLE;
            } else {
                stride_entry.level = replacement_level::NOT_REPLACEABLE;
            }

            stride_entry.confidence = 0;
        }

        entry->confidence = 0;
    }
}

bool berti::get_berti_strides(uint64_t tag, std::vector<stride_entry_type>& strides) const
{
    auto it = berti_table.find(tag);
    if (it == berti_table.end()) return false;

    const auto& entry = it->second;
    strides.clear();

    // Collect valid strides
    for (const auto& stride_entry : entry->strides) {
        if (stride_entry.stride != 0 && stride_entry.level != replacement_level::NOT_REPLACEABLE) {
            strides.push_back(stride_entry);
        }
    }

    // If no confident strides but IP has enough confidence, use percentage-based approach
    if (strides.empty() && entry->confidence >= vberti_config::LAUNCH_THRESHOLD) {
        for (const auto& stride_entry : entry->strides) {
            if (stride_entry.stride != 0) {
                stride_entry_type temp_stride = stride_entry;
                temp_stride.percentage = static_cast<float>(stride_entry.confidence) /
                                       static_cast<float>(entry->confidence) * 100.0f;
                strides.push_back(temp_stride);
            }
        }

        // Sort by percentage and assign levels
        std::sort(strides.begin(), strides.end(), compare_strides_by_percentage);

        for (auto& stride : strides) {
            if (stride.percentage > 80.0f) {
                stride.level = replacement_level::L1_LEVEL;
            } else if (stride.percentage > 35.0f) {
                stride.level = replacement_level::L2_LEVEL;
            } else {
                stride.level = replacement_level::NOT_REPLACEABLE;
            }
        }
    }

    // Sort by priority
    std::sort(strides.begin(), strides.end(), compare_strides_by_priority);

    return !strides.empty();
}

void berti::find_and_update_patterns(uint64_t latency, uint64_t tag, uint64_t cycle, uint64_t line_addr)
{
    std::vector<uint64_t> ips(vberti_config::MAX_HISTORY_IP);
    std::vector<uint64_t> addresses(vberti_config::MAX_HISTORY_IP);

    std::size_t num_entries = get_history_entries(tag, line_addr, latency, cycle, ips, addresses);

    for (std::size_t i = 0; i < num_entries && i < vberti_config::MAX_HISTORY_IP; ++i) {
        // Increase IP confidence on first match
        if (i == 0) {
            increase_berti_confidence(tag);
        }

        // Calculate stride
        uint64_t masked_line_addr = line_addr & vberti_config::ADDRESS_MASK;
        int64_t stride = static_cast<int64_t>(masked_line_addr - addresses[i]);

        // Only consider reasonable stride sizes
        if (std::abs(stride) < (1 << vberti_config::STRIDE_MASK_BITS)) {
            add_to_berti_table(ips[i], stride);
        }
    }
}

bool berti::compare_strides_by_priority(const stride_entry_type& a, const stride_entry_type& b)
{
    // L1 level has highest priority
    if (a.level == replacement_level::L1_LEVEL && b.level != replacement_level::L1_LEVEL) return true;
    if (a.level != replacement_level::L1_LEVEL && b.level == replacement_level::L1_LEVEL) return false;

    // L2 level has second priority
    if (a.level == replacement_level::L2_LEVEL && b.level != replacement_level::L2_LEVEL) return true;
    if (a.level != replacement_level::L2_LEVEL && b.level == replacement_level::L2_LEVEL) return false;

    // L2 replaceable has third priority
    if (a.level == replacement_level::L2_REPLACEABLE && b.level != replacement_level::L2_REPLACEABLE) return true;
    if (a.level != replacement_level::L2_REPLACEABLE && b.level == replacement_level::L2_REPLACEABLE) return false;

    // For same level, prefer smaller absolute stride
    return std::abs(a.stride) < std::abs(b.stride);
}

bool berti::compare_strides_by_percentage(const stride_entry_type& a, const stride_entry_type& b)
{
    if (a.percentage > b.percentage) return true;
    if (a.percentage < b.percentage) return false;

    // For same percentage, prefer smaller absolute stride
    return std::abs(a.stride) < std::abs(b.stride);
}
