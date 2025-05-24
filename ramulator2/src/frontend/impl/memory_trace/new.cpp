#include <filesystem>
#include <iostream>
#include <fstream>

#include "frontend/frontend.h"
#include "base/exception.h"
#include "translation/translation.h"

namespace Ramulator
{

    namespace fs = std::filesystem;

    class CustomTrace2 : public IFrontEnd, public Implementation
    {
        RAMULATOR_REGISTER_IMPLEMENTATION(IFrontEnd, CustomTrace2, "CustomTrace2", "Read/Write DRAM address vector trace with clk support.")

    private:
        struct Trace
        {
            uint64_t clk;
            bool is_write;
            Addr_t addr;
            int source_id;
        };
        std::vector<Trace> m_trace;

        size_t m_trace_length = 0;
        size_t m_curr_trace_idx = 0;

        Logger_t m_logger;
        ITranslation *m_translation; // Translation module instance
        uint64_t clk = 0;            // Current clock cycle

    public:
        void init() override
        {
            std::string trace_path_str = param<std::string>("path").desc("Path to the load store trace file.").required();
            m_clock_ratio = param<uint>("clock_ratio").required();

            m_logger = Logging::create_logger("CustomTrace2");
            m_logger->info("Loading trace file {} ...", trace_path_str);
            init_trace(trace_path_str);
            m_logger->info("Loaded {} lines.", m_trace.size());

            m_translation = create_child_ifce<ITranslation>(); // Initialize translation module
        };

        void tick() override
        {

            const Trace &t = m_trace[m_curr_trace_idx];

            // Wait if trace is for future clk
            if (t.clk < clk)
            {
                clk++;
                return;
            }

            Request req(t.addr, t.is_write ? Request::Type::Write : Request::Type::Read, t.source_id);
            vpn_access_count[req.addr >> 12]++;

            if (!m_translation->translate(req))
            {
                m_curr_trace_idx++;
                return; // Skip if translation fails
            }

            if (!m_memory_system->send(req))
            {
                return; // Stall if request not accepted
            }

            m_curr_trace_idx++;

            // Check if next request has the same clock, only then skip clock increment
            if (m_curr_trace_idx < m_trace.size() && m_trace[m_curr_trace_idx].clk == clk)
            {
                return; // Do not increment clock
            }

            clk++; // Increment clock if no more requests for this clock
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
            int line_num = 0;
            while (std::getline(trace_file, line))
            {
                line_num++;
                std::vector<std::string> tokens;
                tokenize(tokens, line, " ");

                if (tokens.size() != 4)
                {
                    m_logger->error("Line {}: Expected 4 tokens, got {} -> '{}'", line_num, tokens.size(), line);
                    throw ConfigurationError("Trace {} format invalid at line {}!", file_path_str, line_num);
                }

                uint64_t clk;
                try
                {
                    clk = std::stoull(tokens[0]);
                }
                catch (...)
                {
                    throw ConfigurationError("Invalid clock at line {}: '{}'", line_num, tokens[0]);
                }

                bool is_write;
                if (tokens[1] == "W")
                    is_write = true;
                else if (tokens[1] == "R")
                    is_write = false;
                else
                    throw ConfigurationError("Invalid access type at line {}: '{}'", line_num, tokens[1]);

                Addr_t addr;
                try
                {
                    addr = std::stoll(tokens[2]);
                }
                catch (...)
                {
                    throw ConfigurationError("Invalid address at line {}: '{}'", line_num, tokens[2]);
                }

                int source_id;
                try
                {
                    source_id = std::stoi(tokens[3]);
                }
                catch (...)
                {
                    throw ConfigurationError("Invalid source ID at line {}: '{}'", line_num, tokens[3]);
                }

                m_trace.push_back({clk, is_write, addr, source_id});
            }

            trace_file.close();
            m_trace_length = m_trace.size();
        };

        bool is_finished() override
        {
            return m_curr_trace_idx >= m_trace.size();
        }
    };

} // namespace Ramulator