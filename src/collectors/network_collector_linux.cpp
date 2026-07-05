#include "collectors/io_collector.h"

#include <fstream>
#include <sstream>
#include <string>

namespace perfm
{
namespace
{
class network_collector final : public collector
{
public:
    void start(std::uint64_t pid) override
    {
        start(pid, collector_scope::process_tree);
    }

    void start(std::uint64_t, collector_scope scope) override
    {
        scope_ = scope;
        const auto counters = read_system_counters();
        last_read_ = counters.read_bytes;
        last_write_ = counters.write_bytes;
    }

    std::vector<metric_value> sample() override
    {
        if (scope_ == collector_scope::process_only)
        {
            return {
                make_unsupported_metric("network_read_bytes", metric_unit::bytes),
                make_unsupported_metric("network_write_bytes", metric_unit::bytes),
            };
        }

        const auto counters = read_system_counters();
        if (!counters.has_value)
        {
            return {
                make_error_metric("network_read_bytes", metric_unit::bytes, "unavailable"),
                make_error_metric("network_write_bytes", metric_unit::bytes, "unavailable"),
            };
        }

        const auto read_delta = counters.read_bytes >= last_read_ ? counters.read_bytes - last_read_ : 0;
        const auto write_delta = counters.write_bytes >= last_write_ ? counters.write_bytes - last_write_ : 0;
        last_read_ = counters.read_bytes;
        last_write_ = counters.write_bytes;

        return {
            make_metric("network_read_bytes", metric_unit::bytes, static_cast<double>(read_delta)),
            make_metric("network_write_bytes", metric_unit::bytes, static_cast<double>(write_delta)),
        };
    }

    void stop() override
    {
    }

private:
    struct network_counters
    {
        unsigned long long read_bytes{0};
        unsigned long long write_bytes{0};
        bool has_value{false};
    };

    static network_counters read_system_counters()
    {
        network_counters total;
        std::ifstream input("/proc/net/dev");
        std::string line;
        while (std::getline(input, line))
        {
            const auto colon = line.find(':');
            if (colon == std::string::npos)
            {
                continue;
            }

            auto interface_name = line.substr(0, colon);
            interface_name.erase(0, interface_name.find_first_not_of(" \t"));
            interface_name.erase(interface_name.find_last_not_of(" \t") + 1);
            if (interface_name == "lo")
            {
                continue;
            }

            std::istringstream fields(line.substr(colon + 1));
            unsigned long long receive_bytes = 0;
            unsigned long long ignored = 0;
            unsigned long long transmit_bytes = 0;
            fields >> receive_bytes;
            for (int index = 0; index < 7; ++index)
            {
                fields >> ignored;
            }
            fields >> transmit_bytes;
            if (!fields.fail())
            {
                total.read_bytes += receive_bytes;
                total.write_bytes += transmit_bytes;
                total.has_value = true;
            }
        }
        return total;
    }

    collector_scope scope_{collector_scope::process_tree};
    unsigned long long last_read_{0};
    unsigned long long last_write_{0};
};
}

collector_ptr make_network_collector()
{
    return std::make_unique<network_collector>();
}
}
