#include "collectors/io_collector.h"

#include <ifaddrs.h>
#include <net/if.h>
#include <net/if_dl.h>

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
        ifaddrs* interfaces = nullptr;
        if (getifaddrs(&interfaces) != 0 || interfaces == nullptr)
        {
            return total;
        }

        for (const auto* current = interfaces; current != nullptr; current = current->ifa_next)
        {
            if (current->ifa_addr == nullptr || current->ifa_addr->sa_family != AF_LINK ||
                (current->ifa_flags & IFF_LOOPBACK) != 0 || (current->ifa_flags & IFF_UP) == 0)
            {
                continue;
            }

            const auto* data = reinterpret_cast<const if_data*>(current->ifa_data);
            if (data == nullptr)
            {
                continue;
            }

            total.read_bytes += data->ifi_ibytes;
            total.write_bytes += data->ifi_obytes;
            total.has_value = true;
        }

        freeifaddrs(interfaces);
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
