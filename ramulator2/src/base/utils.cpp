#include "base/utils.h"
#include <cstdint>
#include <stdexcept>
#include <iomanip> 
#include <algorithm>
#include <set>
#include <climits>
namespace Ramulator
{

  size_t parse_capacity_str(std::string size_str)
  {
    std::string suffixes[3] = {"KB", "MB", "GB"};
    for (int i = 0; i < 3; i++)
    {
      std::string suffix = suffixes[i];
      if (size_str.find(suffix) != std::string::npos)
      {
        size_t size = std::stoull(size_str);
        size = size << (10 * (i + 1));
        return size;
      }
    }
    return 0;
  }

  size_t parse_frequency_str(std::string size_str)
  {
    std::string suffixes[2] = {"MHz", "GHz"};
    for (int i = 0; i < 2; i++)
    {
      std::string suffix = suffixes[i];
      if (size_str.find(suffix) != std::string::npos)
      {
        size_t freq = std::stoull(size_str);
        freq = freq << (10 * i);
        return freq;
      }
    }
    return 0;
  }

  uint64_t JEDEC_rounding(float t_ns, int tCK_ps)
  {
    // Turn timing in nanosecond to picosecond
    uint64_t t_ps = t_ns * 1000;

    // Apply correction factor 974
    uint64_t nCK = ((t_ps * 1000 / tCK_ps) + 974) / 1000;
    return nCK;
  }

  uint64_t JEDEC_rounding_DDR5(float t_ns, int tCK_ps)
  {
    // Turn timing in nanosecond to picosecond
    uint64_t t_ps = t_ns * 1000;

    // Apply correction factor 997 and round up
    uint64_t nCK = ((t_ps * 997 / tCK_ps) + 1000) / 1000;
    return nCK;
  }

  int slice_lower_bits(uint64_t &addr, int bits)
  {
    int lbits = addr & ((1 << bits) - 1);
    addr >>= bits;
    return lbits;
  }

  void tokenize(std::vector<std::string> &tokens, std::string line, std::string delim)
  {
    size_t pos = 0;
    size_t last_pos = 0;
    while ((pos = line.find(delim, last_pos)) != std::string::npos)
    {
      tokens.push_back(line.substr(last_pos, pos - last_pos));
      last_pos = pos + 1;
    }
    tokens.push_back(line.substr(last_pos));
  }

  void printAddressBitsDetails()
  {
    std::cout << "Using Global Address Bits in CustomTranslation:\n";
    for (size_t i = 0; i < global_addr_bits.size(); i++)
    {
      std::cout << "Level " << i << " uses " << global_addr_bits[i] << " bits.\n";
    }
  }

  void printAccessCounts()
  {
    std::cout << "Printing Access Counts:\n";
    for (const auto &vp : vpn_access_count)
    {
      std::cout << "VPN " << vp.first << " has " << vp.second << " accesses.\n";
    }
  }

  void printTotalCacheCounts()
  {
    std::cout << "Printing Total Cache Requests: " << total_cache_req << "\n";

    for (const auto &vp : top_cache)

    {
      std::cout << "VPN is " << vp << "\n";
    }
  }

  void initialize_core_channel_latency() {
    const int latency_pattern[5] = {20, 30, 60, 100, 130};

    for (int core = 0; core < 8; ++core) {
        for (int ch = 0; ch < 8; ++ch) {
            int offset = std::abs(core - ch);
            if (offset < 5) {
                core_channel_latency_matrix[core][ch] = latency_pattern[offset];
            } else {
                // Mirror the pattern for symmetry beyond offset 4
                int mirrored_offset = 8 - offset;
                core_channel_latency_matrix[core][ch] = latency_pattern[mirrored_offset];
            }
        }
    }



    // Print the core-channel latency matrix
    // std::cout << "Core-to-Channel Latency Matrix:\n";
    // for (int core = 0; core < 8; ++core) {
    //     std::cout << "Core " << core << ": ";
    //     for (int ch = 0; ch < 8; ++ch) {
    //         std::cout << std::setw(3) << core_channel_latency_matrix[core][ch] << " ";
    //     }
    //     std::cout << "\n";
    // }
    
  }

} // namespace Ramulator