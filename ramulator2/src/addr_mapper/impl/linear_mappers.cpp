#include <vector>
#include "base/base.h"
#include "base/utils.h"
#include "dram/dram.h"
#include "addr_mapper/addr_mapper.h"
#include "memory_system/memory_system.h"

namespace Ramulator
{

  class LinearMapperBase : public IAddrMapper
  {
  public:
    IDRAM *m_dram = nullptr;

    int m_num_levels = -1;        // How many levels in the hierarchy?
    std::vector<int> m_addr_bits; // How many address bits for each level in the hierarchy?
    Addr_t m_tx_offset = -1;

    int m_col_bits_idx = -1;
    int m_row_bits_idx = -1;

  protected:
    void setup(IFrontEnd *frontend, IMemorySystem *memory_system)
    {
      m_dram = memory_system->get_ifce<IDRAM>();
      const auto &count = m_dram->m_organization.count;

      // ✅ Populate global address bits vector (without modifying existing logic)
      if (global_addr_bits.empty())
      { // Ensure it initializes only once
        // std::cout << "Initializing Global Address Bits Structure..." << std::endl;
        global_addr_bits.resize(count.size());

        for (size_t level = 0; level < count.size(); level++)
        {
          global_addr_bits[level] = calc_log2(count[level]);
        }

        // Adjust last level (column bits) based on prefetch size
        global_addr_bits[count.size() - 1] -= calc_log2(m_dram->m_internal_prefetch_size);

        // std::cout << "Global Address Bits (Populated in setup()): ";
        for (size_t i = 0; i < global_addr_bits.size(); i++)
        {
          // std::cout << global_addr_bits[i] << " ";
        }
        // std::cout << std::endl;
      }

      // Populate m_addr_bits vector with the number of address bits for each level in the hierachy
      
      m_num_levels = count.size();
      m_addr_bits.resize(m_num_levels);
      for (size_t level = 0; level < m_addr_bits.size(); level++)
      {
        m_addr_bits[level] = calc_log2(count[level]);
      }

      // Last (Column) address have the granularity of the prefetch size
      m_addr_bits[m_num_levels - 1] -= calc_log2(m_dram->m_internal_prefetch_size);

      int tx_bytes = m_dram->m_internal_prefetch_size * m_dram->m_channel_width / 8;
      m_tx_offset = calc_log2(tx_bytes);
      // std::cout << "TX offset: " << m_tx_offset << std::endl;

      // Determine where are the row and col bits for ChRaBaRoCo and RoBaRaCoCh
      try
      {
        m_row_bits_idx = m_dram->m_levels("row");
      }
      catch (const std::out_of_range &r)
      {
        throw std::runtime_error(fmt::format("Organization \"row\" not found in the spec, cannot use linear mapping!"));
      }

      // Assume column is always the last level
      m_col_bits_idx = m_num_levels - 1;
    }
  };

  class ChRaBaRoCo final : public LinearMapperBase, public Implementation
  {
    RAMULATOR_REGISTER_IMPLEMENTATION(IAddrMapper, ChRaBaRoCo, "ChRaBaRoCo", "Applies a trival mapping to the address.");

  public:
    bool mapping_details_printed1 = false;
    void init() override {};

    void setup(IFrontEnd *frontend, IMemorySystem *memory_system) override
    {
      LinearMapperBase::setup(frontend, memory_system);
    }

    void apply(Request &req) override
    {
      req.addr_vec.resize(m_num_levels, -1);
      Addr_t addr = req.addr >> m_tx_offset;
      for (int i = m_addr_bits.size() - 1; i >= 0; i--)
      {
        req.addr_vec[i] = slice_lower_bits(addr, m_addr_bits[i]);
      }

      if (!mapping_details_printed1)
      {
        // print_address_mapping_details1(req.addr_vec, req);
        mapping_details_printed1 = true; // Set the flag to true after printing
      }
    }

    void print_address_mapping_details1(const std::vector<int> &addr_vec, Request &req) const
    {
      std::cout << "Virtual Page Number:" << req.vpage << std::endl;
      // std::cout << "Physical Page Number:" << page_table[req.vpage] << std::endl;
      std::cout << "Address Mapping Details for ChRaBaRoCo:" << req.addr << std::endl;

      // Print details for each DRAM hierarchy level
      std::cout << "Channel: " << addr_vec[0] << " (using " << m_addr_bits[0] << " bits)" << std::endl;
      std::cout << "Rank: " << addr_vec[1] << " (using " << m_addr_bits[1] << " bits)" << std::endl;
      std::cout << "Bank Group: " << addr_vec[2] << " (using " << m_addr_bits[2] << " bits)" << std::endl;
      std::cout << "Bank: " << addr_vec[3] << " (using " << m_addr_bits[3] << " bits)" << std::endl;
      std::cout << "Row: " << addr_vec[m_row_bits_idx] << " (using " << m_addr_bits[m_row_bits_idx] << " bits)" << std::endl;
      std::cout << "Column: " << addr_vec[m_col_bits_idx] << " (using " << m_addr_bits[m_col_bits_idx] << " bits)" << std::endl;
    }
  };

  class RoBaRaCoCh final : public LinearMapperBase, public Implementation
  {
    RAMULATOR_REGISTER_IMPLEMENTATION(IAddrMapper, RoBaRaCoCh, "RoBaRaCoCh", "Applies a RoBaRaCoCh mapping to the address.");

  public:
    bool mapping_details_printed = false;
    void init() override {};

    void setup(IFrontEnd *frontend, IMemorySystem *memory_system) override
    {
      LinearMapperBase::setup(frontend, memory_system);
    }

    void apply(Request &req) override
    {
      // std::cout << "levels = " << m_num_levels << std::endl;
      req.addr_vec.resize(m_num_levels, -1);
      Addr_t addr = req.addr >> m_tx_offset;

      // std::cout << "addr after offset: " <<addr << std::endl;

      // Extract bits for each level and assign them to the address vector
      req.addr_vec[0] = slice_lower_bits(addr, m_addr_bits[0]);                                           // Channel
      req.addr_vec[m_addr_bits.size() - 1] = slice_lower_bits(addr, m_addr_bits[m_addr_bits.size() - 1]); // Column
      for (int i = 1; i <= m_row_bits_idx; i++)
      {
        req.addr_vec[i] = slice_lower_bits(addr, m_addr_bits[i]);
      }

      if (!mapping_details_printed)
      {
        // print_address_mapping_details(req.addr_vec, req);
        mapping_details_printed = true; // Set the flag to true after printing
      }

      // Print details of each level
    }

    void print_address_mapping_details(const std::vector<int> &addr_vec, Request &req) const
    {
      std::cout << "Virtual Page Number:" << req.vpage << std::endl;
      // std::cout << "Physical Page Number:" << page_table[req.vpage] << std::endl;
      std::cout << "Address Mapping Details for RoBaRaCoCh:" << req.addr << std::endl;

      // Print details for each DRAM hierarchy level
      std::cout << "Channel: " << addr_vec[0] << " (using " << m_addr_bits[0] << " bits)" << std::endl;
      std::cout << "Rank: " << addr_vec[1] << " (using " << m_addr_bits[1] << " bits)" << std::endl;
      std::cout << "Bank Group: " << addr_vec[2] << " (using " << m_addr_bits[2] << " bits)" << std::endl;
      std::cout << "Bank: " << addr_vec[3] << " (using " << m_addr_bits[3] << " bits)" << std::endl;
      std::cout << "Row: " << addr_vec[m_row_bits_idx] << " (using " << m_addr_bits[m_row_bits_idx] << " bits)" << std::endl;
      std::cout << "Column: " << addr_vec[m_col_bits_idx] << " (using " << m_addr_bits[m_col_bits_idx] << " bits)" << std::endl;
    }
  };

  class MOP4CLXOR final : public LinearMapperBase, public Implementation
  {
    RAMULATOR_REGISTER_IMPLEMENTATION(IAddrMapper, MOP4CLXOR, "MOP4CLXOR", "Applies a MOP4CLXOR mapping to the address.");

  public:
    void init() override {};

    void setup(IFrontEnd *frontend, IMemorySystem *memory_system) override
    {
      LinearMapperBase::setup(frontend, memory_system);
    }

    void apply(Request &req) override
    {
      req.addr_vec.resize(m_num_levels, -1);
      Addr_t addr = req.addr >> m_tx_offset;
      req.addr_vec[m_col_bits_idx] = slice_lower_bits(addr, 2);
      for (int lvl = 0; lvl < m_row_bits_idx; lvl++)
        req.addr_vec[lvl] = slice_lower_bits(addr, m_addr_bits[lvl]);
      req.addr_vec[m_col_bits_idx] += slice_lower_bits(addr, m_addr_bits[m_col_bits_idx] - 2) << 2;
      req.addr_vec[m_row_bits_idx] = (int)addr;

      int row_xor_index = 0;
      for (int lvl = 0; lvl < m_col_bits_idx; lvl++)
      {
        if (m_addr_bits[lvl] > 0)
        {
          int mask = (req.addr_vec[m_col_bits_idx] >> row_xor_index) & ((1 << m_addr_bits[lvl]) - 1);
          req.addr_vec[lvl] = req.addr_vec[lvl] xor mask;
          row_xor_index += m_addr_bits[lvl];
        }
      }
    }
  };

} // namespace Ramulator