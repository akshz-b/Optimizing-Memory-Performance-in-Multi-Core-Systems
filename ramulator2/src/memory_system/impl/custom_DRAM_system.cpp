#include "memory_system/memory_system.h"
#include "translation/translation.h"
#include "dram_controller/controller.h"
#include "addr_mapper/addr_mapper.h"
#include "dram/dram.h"
#include "base/utils.h"

namespace Ramulator
{

    class CustomDRAMSystem final : public IMemorySystem, public Implementation
    {
        RAMULATOR_REGISTER_IMPLEMENTATION(IMemorySystem, CustomDRAMSystem, "CustomDRAM", "A Custom DRAM-based memory system.");

    protected:
        Clk_t m_clk = 0;
        IDRAM *m_dram;
        IAddrMapper *m_addr_mapper;
        std::vector<IDRAMController *> m_controllers;

    public:
        int s_num_read_requests = 0;
        int s_num_write_requests = 0;
        int s_num_other_requests = 0;
        std::unordered_map<Addr_t, size_t> vpn_access_counts;

    public:
        void init() override
        {
            // Create device (a top-level node wrapping all channel nodes)
            m_dram = create_child_ifce<IDRAM>();
            m_addr_mapper = create_child_ifce<IAddrMapper>();

            int num_channels = m_dram->get_level_size("channel");

            // Create memory controllers
            for (int i = 0; i < num_channels; i++)
            {
                IDRAMController *controller = create_child_ifce<IDRAMController>();
                controller->m_impl->set_id(fmt::format("Channel {}", i));
                controller->m_channel_id = i;
                m_controllers.push_back(controller);
            }

            m_clock_ratio = param<uint>("clock_ratio").required();

            register_stat(m_clk).name("memory_system_cycles");
            register_stat(s_num_read_requests).name("total_num_read_requests");
            register_stat(s_num_write_requests).name("total_num_write_requests");
            register_stat(s_num_other_requests).name("total_num_other_requests");
        };

        void setup(IFrontEnd *frontend, IMemorySystem *memory_system) override {}

        bool send(Request req) override
        {

            Addr_t vpn = req.v_addr >> 12; // Extract VPN from address
            vpn_access_counts[vpn]++;      // Increment access count for this VPN

            if (is_in_top_cache(vpn))
            {
                DEBUG_LOG(DTRANSLATE, m_logger, "VPN {} is in top cache; skipping translation.", vpn);
                total_cache_req++;

                switch (req.type_id)
                {
                case Request::Type::Read:
                {
                    s_num_read_requests++;
                    break;
                }
                case Request::Type::Write:
                {
                    s_num_write_requests++;
                    break;
                }
                default:
                {
                    s_num_other_requests++;
                    break;
                }
                }

                return false;
            }
            update_top_cache(vpn, vpn_access_counts[vpn]);
            

            m_addr_mapper->apply(req);
            int channel_id = req.addr_vec[0];
            bool is_success = m_controllers[channel_id]->send(req);

            if (is_success)
            {
                switch (req.type_id)
                {
                case Request::Type::Read:
                {
                    s_num_read_requests++;
                    break;
                }
                case Request::Type::Write:
                {
                    s_num_write_requests++;
                    break;
                }
                default:
                {
                    s_num_other_requests++;
                    break;
                }
                }
            }

            return is_success;
        };

        void tick() override
        {
            m_clk++;
            m_dram->tick();
            for (auto controller : m_controllers)
            {
                controller->tick();
            }
        };

        float get_tCK() override
        {
            return m_dram->m_timing_vals("tCK_ps") / 1000.0f;
        }

        // const SpecDef& get_supported_requests() override {
        //   return m_dram->m_requests;
        // };

        void update_top_cache(Addr_t vpn, size_t vpn_total_count)
        {
            // If already in cache, no further update needed.
            if (is_in_top_cache(vpn))
                return;

            // If cache size is less than 4, add this vpn.
            if (top_cache.size() < 4)
            {
                top_cache.push_back(vpn);
            }
            else
            {
                // Find the VPN in the cache with the smallest total access count.
                size_t min_index = 0;
                size_t min_count = std::numeric_limits<size_t>::max();
                for (size_t i = 0; i < top_cache.size(); i++)
                {
                    size_t count = 0;
                    Addr_t cached_vpn = top_cache[i];
                    count = vpn_access_counts[cached_vpn];

                    if (count < min_count)
                    {
                        min_count = count;
                        min_index = i;
                    }
                }
                // If the current vpn's count is higher than the minimum in the cache, replace it.
                if (vpn_total_count > min_count)
                {
                    top_cache[min_index] = vpn;
                }
            }
        }

        // Helper: Check if a given vpn is in the top cache.
        bool is_in_top_cache(Addr_t vpn)
        {
            for (const auto &p : top_cache)
            {
                if (p == vpn)
                    return true;
            }
            return false;
        }
    };

} // namespace
