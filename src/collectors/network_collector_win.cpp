#include "collectors/io_collector.h"

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0600
#endif
#ifndef NTDDI_VERSION
#define NTDDI_VERSION 0x06000000
#endif

#define NOMINMAX
#include <winsock2.h>
#include <windows.h>
#include <netioapi.h>
#include <iphlpapi.h>

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
        MIB_IF_TABLE2* table = nullptr;
        if (GetIfTable2(&table) != NO_ERROR || table == nullptr)
        {
            return total;
        }

        for (ULONG index = 0; index < table->NumEntries; ++index)
        {
            const auto& row = table->Table[index];
            if (row.OperStatus != IfOperStatusUp || row.Type == IF_TYPE_SOFTWARE_LOOPBACK)
            {
                continue;
            }

            total.read_bytes += row.InOctets;
            total.write_bytes += row.OutOctets;
            total.has_value = true;
        }

        FreeMibTable(table);
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
