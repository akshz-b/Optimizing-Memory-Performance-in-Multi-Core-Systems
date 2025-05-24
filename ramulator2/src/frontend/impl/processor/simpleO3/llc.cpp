#include <iostream>
#include "frontend/impl/processor/simpleO3/llc.h"

namespace Ramulator {

SimpleO3LLC::SimpleO3LLC(int latency, int size_bytes, int linesize_bytes, int associativity, int num_mshrs)
  : m_latency(latency), m_size_bytes(size_bytes), m_linesize_bytes(linesize_bytes),
    m_associativity(associativity), m_num_mshrs(num_mshrs) {

  m_logger = Logging::create_logger("SimpleO3LLC");

  m_set_size = m_size_bytes / (m_linesize_bytes * m_associativity);
  m_index_mask = m_set_size - 1;
  m_index_offset = calc_log2(m_linesize_bytes);
  m_tag_offset = calc_log2(m_set_size) + m_index_offset;

  // // Print dummy cache configuration
  // std::cout << "SimpleO3LLC parameters (Passthrough Mode):" << std::endl;
  // std::cout << "  Latency: " << m_latency << std::endl;
  // std::cout << "  Size: " << m_size_bytes << std::endl;
  // std::cout << "  Linesize: " << m_linesize_bytes << std::endl;
  // std::cout << "  Associativity: " << m_associativity << std::endl;
  // std::cout << "  Set size: " << m_set_size << std::endl;
  // std::cout << "  Number of MSHRs: " << m_num_mshrs << std::endl;
  // std::cout << "  Index mask: " << std::hex << m_index_mask << std::dec << std::endl;
  // std::cout << "  Index offset: " << m_index_offset << std::endl;
  // std::cout << "  Tag offset: " << m_tag_offset << std::endl;
}

void SimpleO3LLC::tick() {
  m_clk++;
  // Nothing to do — passthrough mode
}

bool SimpleO3LLC::send(Request req) {
  // Directly forward request to memory
  return m_memory_system->send(req);
}

void SimpleO3LLC::receive(Request& req) {
  // Immediately call the core’s callback
  req.callback(req);
}

// === All other methods below are now dead code ===
// === Keep them if needed for serialization, else delete ===

void SimpleO3LLC::serialize(std::string) { }
void SimpleO3LLC::deserialize(std::string) { }
void SimpleO3LLC::dump_llc() { }

SimpleO3LLC::CacheSet_t& SimpleO3LLC::get_set(Addr_t) {
  throw std::logic_error("LLC caching is disabled in passthrough mode.");
}

SimpleO3LLC::CacheSet_t::iterator SimpleO3LLC::allocate_line(CacheSet_t&, Addr_t) {
  throw std::logic_error("LLC allocate_line called in passthrough mode.");
}

bool SimpleO3LLC::need_eviction(const CacheSet_t&, Addr_t) {
  return false;
}

void SimpleO3LLC::evict_line(CacheSet_t&, CacheSet_t::iterator) {
  // No-op
}

SimpleO3LLC::CacheSet_t::iterator SimpleO3LLC::check_set_hit(CacheSet_t&, Addr_t) {
  return CacheSet_t::iterator();
}

SimpleO3LLC::MSHR_t::iterator SimpleO3LLC::check_mshr_hit(Addr_t) {
  return MSHR_t::iterator();
}

}  // namespace Ramulator
