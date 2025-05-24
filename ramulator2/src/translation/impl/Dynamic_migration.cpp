#include <iostream>
#include <unordered_set>
#include <unordered_map>
#include <vector>
#include <random>
#include "dram/dram.h"
#include "base/base.h"
#include "base/utils.h"
#include "translation/translation.h"
#include "frontend/frontend.h"
#include "memory_system/memory_system.h"
#include <ctime>
#include <cstdint>

namespace Ramulator
{
    class Dynamic_migration : public ITranslation, public Implementation
    {
        RAMULATOR_REGISTER_IMPLEMENTATION(ITranslation, Dynamic_migration, "Dynamic_migration", "Randomly allocate physical pages to virtual pages.");

        IFrontEnd *m_frontend;

    protected:
        std::mt19937_64 m_allocator_rng;

        Addr_t m_max_paddr; // Maximum physical address
        Addr_t m_pagesize;  // Page size in bytes
        int m_offsetbits;   // Number of bits for the page offset
        size_t m_num_pages; // Total number of physical pages

        size_t migration_counter = 0;
        size_t HOT_PAGE_THRESHOLD;
        size_t STABILITY_WINDOW;
        int window_counter = 0;

        size_t m_cost = 0;

        size_t pages_per_channel;

        // Free physical page tracking (divided into 8 partitions for 8 channels)
        std::vector<std::vector<bool>> m_free_physical_pages2d;
        std::vector<size_t> m_num_free_physical_pages_per_part;

        std::unordered_map<Addr_t, Addr_t> global_page_table; // VPN → PPN

        std::unordered_map<Addr_t, Addr_t> reverse_page_table; // PPN → VPN (for fast eviction)

        std::unordered_map<size_t, std::unordered_map<size_t, size_t>> page_access_counts_per_core; // Core-wise page accesses

        // std::unordered_map<size_t, size_t> page_to_channel_mapping;                                 // VPN → Channel

        std::unordered_set<Addr_t> m_reserved_pages; // Reserved pages

        std::unordered_map<Addr_t, int> last_migration_window; // VPN → last migrated window
        int COOLDOWN_WINDOWS;   
        
        double bandwidth;  // Cooldown period in windows

    public:
        void init() override
        {
            // Seed for random number generator (can be specified via config)
            int seed = param<int>("seed").desc("The seed for the random number generator used to allocate pages.").default_val(123);
            m_allocator_rng.seed(seed);

            // Get max physical address and page size from configuration
            m_max_paddr = param<Addr_t>("max_addr").desc("Max physical address of the memory system.").required();
            m_pagesize = param<Addr_t>("pagesize_KB").desc("Pagesize in KB.").default_val(4) << 10;
            m_offsetbits = calc_log2(m_pagesize);

            HOT_PAGE_THRESHOLD = param<size_t>("hot_page_threshold").desc("Threshold for hot pages.").required();
            STABILITY_WINDOW = param<size_t>("window_size").desc("Window size.").required();

            COOLDOWN_WINDOWS = param<int>("cooldown_windows").desc("Number of migration windows to wait before remigrating a page.").default_val(2);
            bandwidth = param<double>("bandwidth_GBps")
                                   .desc("Memory bandwidth in GB/s used to estimate migration cost.")
                                   .default_val(153.0); // ← fallback if not given

            // Calculate total number of physical pages
            m_num_pages = m_max_paddr / m_pagesize;

            // Initialize free physical page tracking
            pages_per_channel = m_num_pages / 8;
            m_free_physical_pages2d = std::vector<std::vector<bool>>(8, std::vector<bool>(pages_per_channel, true));
            m_num_free_physical_pages_per_part = std::vector<size_t>(8, pages_per_channel);

            m_logger = Logging::create_logger("Dynamic_migration");

            m_cost = calculate_migration_cost(m_pagesize,bandwidth); // Example memory bandwidth

            // std::cout << "Total Physical Pages: " << m_num_pages << std::endl;
            // std::cout << "Pages Per Partition: " << pages_per_channel << std::endl;
        };

        bool translate(Request &req) override
        {
            req.v_addr = req.addr;                 // Store the virtual address
            Addr_t vpn = req.addr >> m_offsetbits; // Extract VPN from address
            req.vpage = vpn;                       // Save VPN

            // Update access count for this VPN by the requesting core
            // std::cout << " core id " << req.source_id << " VPN " << vpn << std::endl;
            page_access_counts_per_core[vpn][req.source_id]++;

            migration_counter++;
            if (migration_counter == STABILITY_WINDOW)
            {
                migrate_pages();
                migration_counter = 0;
            }

            // If the VPN is already translated, return the existing mapping
            if (global_page_table.find(vpn) != global_page_table.end())
            {
                Addr_t p_addr = (global_page_table[vpn] << m_offsetbits) | (req.addr & ((1 << m_offsetbits) - 1));
                req.addr = p_addr;
                DEBUG_LOG(DTRANSLATE, m_logger, "Translated Addr {}, VPN {} to Addr {}, PPN {}.", req.addr, vpn, p_addr, global_page_table[vpn]);
                return true;
            }

            // Otherwise, assign a new physical page
            Addr_t ppn;
            int ch = find_best_channel(req.source_id); // Choose best channel (here a simple hash is used)

            if (m_num_free_physical_pages_per_part[ch] > 0) // Free pages available in chosen partition
            {
                bool flag = true;
                while (flag)
                {
                    // Select a random page in the partition: random offset within partition plus partition start offset
                    ppn = (m_allocator_rng() % pages_per_channel) + (ch * pages_per_channel);
                    if (m_free_physical_pages2d[ch][ppn % pages_per_channel])
                    {
                        flag = false;
                        m_free_physical_pages2d[ch][ppn % pages_per_channel] = false;
                        m_num_free_physical_pages_per_part[ch]--;
                    }
                }
            }
            else
            {
                // If no free page exists in the partition, evict a page and replace it
                ppn = evict_and_replace_page(ch);
            }

            // Update mappings: VPN to PPN and reverse
            global_page_table[vpn] = ppn;
            reverse_page_table[ppn] = vpn;
            page_to_channel_mapping[vpn] = ch;

            // Compute the translated physical address (preserving offset bits)
            Addr_t p_addr = (global_page_table[vpn] << m_offsetbits) | (req.addr & ((1 << m_offsetbits) - 1));
            DEBUG_LOG(DTRANSLATE, m_logger, "Translated Addr {}, VPN {} to Addr {}, PPN {}.", req.addr, vpn, p_addr, global_page_table[vpn]);
            // m_logger->info("Translated Addr {}, VPN {} to Addr {}, PPN {}, channel: {}", req.addr, vpn, p_addr, global_page_table[vpn], ch);
            req.addr = p_addr;
            return true;
        };

        // Migrate hot pages based on latency gain estimates
        void migrate_pages()
        {
            // std::cout<<"Migration window: " << window_counter << std::endl;
            // std::cout << "Migrating called..." << std::endl;
            // std::cout << "Window: " << window_counter << std::endl;
            for (const auto &entry : global_page_table)
            {
                size_t vpn = entry.first;
                Addr_t ppn = entry.second;

                if (!is_hot_page(vpn))
                    continue;

                if (vpn == 0)
                    continue;

                size_t most_frequent_core = find_most_frequent_core(vpn);
                size_t best_channel = find_best_channel(most_frequent_core);
                size_t current_channel = page_to_channel_mapping[vpn];

                if (best_channel == current_channel)
                    continue;

                // Cooldown check
                if (last_migration_window.find(vpn) != last_migration_window.end())
                {
                    int last_window = last_migration_window[vpn];
                    if ((window_counter - last_window) < COOLDOWN_WINDOWS)
                    {
                        m_logger->info("VPN {} is cooling down (last migrated in window {}).", vpn, last_window);
                        continue;
                    }
                }

                double gain = estimate_latency_gain(vpn, most_frequent_core);
                double cost = m_cost;
                gain /= 1000;

                if (gain > cost)
                {
                    ppn = find_random_free_page(best_channel);
                    global_page_table[vpn] = ppn;
                    page_to_channel_mapping[vpn] = best_channel;
                    m_logger->info("Migrated VPN {} to a new page in channel {}. in window {}", vpn, best_channel, window_counter);
                    // m_logger->info("Migrated VPN {} to a new page in channel {}.", vpn, page_to_channel_mapping[vpn]);
                    last_migration_window[vpn] = window_counter;
                    migrations++;
                }
                else
                {
                    m_logger->info("Migration not beneficial for VPN {}.", vpn);
                    std::cout << "Cost: " << cost << ", Gain: " << gain << std::endl;
                }
            }

            window_counter++;

            reset_all_page_access_counts();
        }

        size_t find_most_frequent_core(Addr_t vpn)
        {
            size_t most_frequent_core = 0;
            size_t max_access_count = 0;

            if (page_access_counts_per_core.find(vpn) != page_access_counts_per_core.end())
            {
                for (const auto &entry : page_access_counts_per_core[vpn])
                {
                    if (entry.second > max_access_count)
                    {
                        max_access_count = entry.second;
                        most_frequent_core = entry.first;
                    }
                }
            }
            return most_frequent_core;
        }

        size_t find_best_channel(size_t core)
        {
            size_t best_channel = 0;
            size_t min_latency = SIZE_MAX;

            for (size_t channel = 0; channel < 8; channel++)

            {
                if (core_channel_latency_matrix[core][channel] < min_latency)
                {
                    min_latency = core_channel_latency_matrix[core][channel];
                    best_channel = channel;
                }
            }
            return best_channel;
        }

        Addr_t find_random_free_page(int ch)
        {

            // Check if any free page exists in this partition.
            if (m_num_free_physical_pages_per_part[ch] == 0)
            {
                // Fallback to eviction (this should normally not happen if the caller checks first)
                return evict_and_replace_page(ch);
            }

            Addr_t ppn;
            while (true)
            {
                // Select a random offset within the partition and add the partition's starting offset.
                ppn = (m_allocator_rng() % pages_per_channel) + (ch * pages_per_channel);
                // Use modulo to index into the partition vector.
                if (m_free_physical_pages2d[ch][ppn % pages_per_channel])
                {
                    m_free_physical_pages2d[ch][ppn % pages_per_channel] = false;
                    m_num_free_physical_pages_per_part[ch]--;
                    return ppn;
                }
            }
        }

        Addr_t evict_and_replace_page(int ch)
        {
            size_t pages_per_channel = m_num_pages / 8;
            // Randomly select a victim page within the channel partition
            Addr_t victim_ppn = (m_allocator_rng() % pages_per_channel) + (ch * pages_per_channel);

            // Use the reverse page table for an O(1) lookup of the corresponding VPN
            Addr_t victim_vpn = reverse_page_table[victim_ppn];

            // Remove the victim's mapping from all tables
            global_page_table.erase(victim_vpn);
            reverse_page_table.erase(victim_ppn);
            page_to_channel_mapping.erase(victim_vpn);

            std::cout << "Evicted VPN: " << victim_vpn << " -> PPN: " << victim_ppn << " from channel " << ch << std::endl;

            // Mark the victim page as free again
            m_free_physical_pages2d[ch][victim_ppn % pages_per_channel] = true;
            m_num_free_physical_pages_per_part[ch]++;

            // Allocate and return a new free page from this partition
            return find_random_free_page(ch);
        }

        // Determine if a page is "hot" (i.e., frequently accessed) based on a threshold.
        bool is_hot_page(size_t vpn)
        {
            size_t total_access_count = 0;
            for (const auto &[core_id, count] : page_access_counts_per_core[vpn])
            {
                total_access_count += count;
            }
            return total_access_count >= HOT_PAGE_THRESHOLD;
        }

        double estimate_latency_gain(size_t page_id, size_t current_core)
        {
            size_t current_channel = page_to_channel_mapping[page_id]; // Track assigned channel
            size_t best_channel = std::distance(core_channel_latency_matrix[current_core],
                                                std::min_element(core_channel_latency_matrix[current_core],
                                                                 core_channel_latency_matrix[current_core] + 8));

            double current_latency = core_channel_latency_matrix[current_core][current_channel];
            double new_latency = core_channel_latency_matrix[current_core][best_channel];

            // 📌 Predict future accesses using historical access count
            size_t recent_accesses = page_access_counts_per_core[page_id][current_core];
            size_t predicted_accesses = recent_accesses * FUTURE_ACCESS_FACTOR; // Multiplier for future prediction

            double total_gain = (current_latency - new_latency) * 2 * predicted_accesses;

            return total_gain; // Return only the gain
        }
        double calculate_migration_cost(size_t page_size, double memory_bandwidth)
        {
            // Convert page size from KB to bytes
            double page_size_in_bytes = static_cast<double>(page_size) * 1024.0;

            // Convert bandwidth from GB/s to bytes/second
            double bandwidth_in_bytes_per_second = memory_bandwidth * 1024.0 * 1024.0 * 1024.0;

            // Calculate cost in seconds
            double cost_in_seconds = page_size_in_bytes / bandwidth_in_bytes_per_second;

            // Convert cost to microseconds
            double cost_in_microseconds = cost_in_seconds * 1e6;

            return cost_in_microseconds;
        }
        void reset_all_page_access_counts()
        {
            page_access_counts_per_core.clear();
        }
        bool reserve(const std::string &type, Addr_t addr) override
        {
            Addr_t ppn = addr >> m_offsetbits;
            m_reserved_pages.insert(ppn);
            return true;
        };

        Addr_t get_max_addr() override
        {
            return m_max_paddr;
        };
    };

} // namespace Ramulator
