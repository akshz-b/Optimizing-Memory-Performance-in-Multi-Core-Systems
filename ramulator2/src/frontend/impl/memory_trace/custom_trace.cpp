#include <filesystem>
#include <iostream>
#include <fstream>

#include "frontend/frontend.h"
#include "base/exception.h"
#include "translation/translation.h" // Added Translation Module

namespace Ramulator
{

  namespace fs = std::filesystem;

  class CustomTrace : public IFrontEnd, public Implementation
  {
    RAMULATOR_REGISTER_IMPLEMENTATION(IFrontEnd, CustomTrace, "CustomTrace", "Read/Write DRAM address vector trace.")

  private:
    struct Trace
    {
      bool is_write;
      Addr_t addr;
      int source_id;
    };
    std::vector<Trace> m_trace;

    size_t m_trace_length = 0;
    size_t m_curr_trace_idx = 0;

    Logger_t m_logger;

    ITranslation *m_translation; // Translation module instance

  public:
    void init() override
    {
      std::string trace_path_str = param<std::string>("path").desc("Path to the load store trace file.").required();
      m_clock_ratio = param<uint>("clock_ratio").required();

      m_logger = Logging::create_logger("CustomTrace");
      m_logger->info("Loading trace file {} ...", trace_path_str);
      init_trace(trace_path_str);
      m_logger->info("Loaded {} lines.", m_trace.size());

      m_translation = create_child_ifce<ITranslation>(); // Initialize translation module
    };

    void tick() override
    {
      const Trace &t = m_trace[m_curr_trace_idx];
      Request req(t.addr, t.is_write ? Request::Type::Write : Request::Type::Read, t.source_id);

      vpn_access_count[req.addr >> 12]++; // Update VPN access count

      // Perform address translation on the request
      if (!m_translation->translate(req))
      {
        m_curr_trace_idx = m_curr_trace_idx + 1;
        return;
        throw std::runtime_error("Address translation failed");
      }

      // Send the translated request and increment m_curr_trace_idx only if send() returns true
      if (m_memory_system->send(req))
      {
        m_curr_trace_idx = m_curr_trace_idx + 1;
      }
      
    }

  private:
    void init_trace(const std::string &file_path_str)
    {
      fs::path trace_path(file_path_str);
      if (!fs::exists(trace_path))
      {
        throw ConfigurationError("Trace {} does not exist!", file_path_str);
      }

      std::ifstream trace_file(trace_path);
      if (!trace_file.is_open())
      {
        throw ConfigurationError("Trace {} cannot be opened!", file_path_str);
      }

      std::string line;
      while (std::getline(trace_file, line))
      {
        std::vector<std::string> tokens;
        tokenize(tokens, line, " ");

        // TODO: Add line number here for better error messages
        if (tokens.size() != 3)
        {
          throw ConfigurationError("Trace {} format invalid!", file_path_str);
        }

        bool is_write = false;
        if (tokens[0] == "R")
        {
          is_write = false;
        }
        else if (tokens[0] == "W")
        {
          is_write = true;
        }
        else
        {
          throw ConfigurationError("Trace {} format invalid!", file_path_str);
        }

        Addr_t addr = std::stoll(tokens[1]);
        int source_id = std::stoi(tokens[2]);

        m_trace.push_back({is_write, addr, source_id});
      }

      trace_file.close();

      m_logger->info("Loaded {} requests from trace file.", m_trace.size());

      m_trace_length = m_trace.size();
    };

    // TODO: FIXME
    bool is_finished() override
    {
      return m_curr_trace_idx >= m_trace.size();
    }
  };

} // namespace Ramulator
