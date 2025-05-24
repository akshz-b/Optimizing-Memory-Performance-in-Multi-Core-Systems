#ifndef RAMULATOR_BASE_UTILS_H
#define RAMULATOR_BASE_UTILS_H

#include <string>
#include <vector>
#include <cstdint>
#include <unordered_map>
#include <iostream>

namespace Ramulator
{
  // // Core-channel latency matrix
  // inline const std::vector<std::vector<size_t>> core_channel_latency_matrix = {
  //     // Channels: 0      1      2      3      4      5      6      7
  //     {50, 60, 80, 90, 10, 110, 120, 130}, // Core 0
  //     {50, 60, 80, 90, 100, 110, 120, 130}, // Core 1
  //     {80, 90, 50, 60, 10, 110, 120, 130}, // Core 2
  //     {80, 90, 50, 60, 100, 110, 120, 130}, // Core 3
  //     {100, 110, 120, 130, 50, 60, 80, 90}, // Core 4
  //     {100, 110, 120, 130, 50, 60, 80, 90}, // Core 5
  //     {120, 130, 100, 110, 80, 90, 50, 60}, // Core 6
  //     {120, 130, 100, 110, 80, 90, 50, 60}, // Core 7
  //     {60, 50, 90, 80, 110, 100, 130, 120}, // Core 8
  //     {60, 50, 90, 80, 110, 100, 130, 120}, // Core 9
  //     {90, 80, 60, 50, 130, 120, 110, 100}, // Core 10
  //     {90, 80, 60, 50, 130, 120, 110, 100}, // Core 11
  //     {110, 100, 130, 120, 60, 50, 90, 80}, // Core 12
  //     {110, 100, 130, 120, 60, 50, 90, 80}, // Core 13
  //     {130, 120, 110, 100, 90, 80, 60, 50}, // Core 14
  //     {130, 120, 110, 100, 90, 80, 60, 50}  // Core 15
  // };

  // Core-to-Channel Latency Matrix (filled dynamically)
  inline int core_channel_latency_matrix[8][8];

  // Min-latency channel for each core (precomputed)
  inline int min_latency_channel[8];

  // Fills the latency matrix and min-latency table
  void initialize_core_channel_latency();

  // Global data structure (Page Table)
  inline std::unordered_map<size_t, size_t> page_table;

  // Physical address bits per level
  inline std::vector<int> global_addr_bits;

  inline int replicated_access_count = 0;
  inline int migrations = 0;

  // Global data structure to track page access counts by each core for each page
  // Outer map key is page_id -> Inner map (core_id -> access_count)
  inline std::unordered_map<size_t, std::unordered_map<size_t, size_t>> page_core_access_counts;

  // ✅ Track recent page access timestamps
  inline std::unordered_map<size_t, std::vector<size_t>> page_access_timestamps;

  // ✅ Track page to channel mapping
  inline std::unordered_map<size_t, size_t> page_to_channel_mapping;

  // ✅ Track VPN access count
  inline std::unordered_map<size_t, size_t> vpn_access_count;

  // ✅ Track VPN in memory
  inline std::unordered_map<size_t, bool> vpn_in_memory;

  // top cache
  inline std::vector<size_t> top_cache;

  inline size_t total_cache_req = 0;

  inline size_t overall_latency = 0;
  inline size_t overall_request = 0;
  inline size_t avg_latency = 0;

  // ✅ Constants for migration policy
  // constexpr size_t HOT_PAGE_THRESHOLD = 100;  // Number of accesses to qualify as "hot"
  // constexpr size_t STABILITY_WINDOW = 500;   // Sliding window size (cycles)
  constexpr size_t MIGRATION_COOLDOWN = 5000; // Minimum cycles before re-migrating
  constexpr size_t FUTURE_ACCESS_FACTOR = 1;  // Multiplier for future prediction

  inline std::vector<bool> m_free_physical_pages(33554432, true); // All pages initialized to true
  inline static size_t m_num_free_physical_pages = 33554432;      // Total number of pages

  // inline std::vector<std::vector<bool>> m_free_physical_pages2d(8, std::vector<bool>(33554432 / 8, true));
  // inline std::vector<size_t> m_num_free_physical_pages_per_part(8, 33554432 / 8);

  void printAddressBitsDetails();

  void printAccessCounts();

  void printTotalCacheCounts();

  /************************************************
   *     Utility Functions for Parsing Configs
   ***********************************************/

  /**
   * @brief    Parse capacity strings (e.g., KB, MB) into the number of bytes
   *
   * @param    size_str       A capacity string (e.g., "8KB", "64MB").
   * @return   size_t         The number of bytes.
   */
  size_t parse_capacity_str(std::string size_str);

  /**
   * @brief    Parse frequency strings (e.g., MHz, GHz) into MHz
   *
   * @param    size_str       A capacity string (e.g., "4GHz", "3500MHz").
   * @return   size_t         The number of bytes.
   */
  size_t parse_frequency_str(std::string size_str);

  /**
   * @brief Convert a timing constraint in nanoseconds into number of cycles according to JEDEC convention.
   *
   * @param t_ns      Timing constraint in nanoseconds
   * @param tCK_ps    Clock cycle in picoseconds
   * @return uint64_t Number of cycles
   */
  uint64_t JEDEC_rounding(float t_ns, int tCK_ps);

  /**
   * @brief Convert a timing constraint in nanoseconds into number of cycles according to JEDEC DDR5 convention.
   *
   * @param t_ns      Timing constraint in nanoseconds
   * @param tCK_ps    Clock cycle in picoseconds
   * @return uint64_t Number of cycles
   */
  uint64_t JEDEC_rounding_DDR5(float t_ns, int tCK_ps);

  /************************************************
   *       Bitwise Operations for Integers
   ***********************************************/

  /**
   * @brief Calculate how many bits are needed to store val
   *
   * @tparam Integral_t
   * @param val
   * @return Integral_t
   */
  template <typename Integral_t>
  Integral_t calc_log2(Integral_t val)
  {
    static_assert(std::is_integral_v<Integral_t>, "Only integral types are allowed for bitwise operations!");

    Integral_t n = 0;
    while ((val >>= 1))
    {
      n++;
    }
    return n;
  };

  /**
   * @brief Slice the lest significant num_bits from addr and return these bits. The originial addr value is modified.
   *
   * @tparam Integral_t
   * @param addr
   * @param num_bits
   * @return Integral_t
   */
  template <typename Integral_t>
  Integral_t slice_lower_bits(Integral_t &addr, int num_bits)
  {
    static_assert(std::is_integral_v<Integral_t>, "Only integral types are allowed for bitwise operations!");

    Integral_t lbits = addr & ((1 << num_bits) - 1);
    addr >>= num_bits;
    return lbits;
  };

  /************************************************
   *                Tokenization
   ***********************************************/
  void tokenize(std::vector<std::string> &tokens, std::string line, std::string delim);

} // namespace Ramulator

#endif // RAMULATOR_BASE_UTILS_H