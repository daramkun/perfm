#include "collectors/io_collector.h"
#include "process/process_tree.h"

#define NOMINMAX
#include <windows.h>

namespace perfm
{
namespace
{
class file_io_collector final : public collector
{
public:
    void start(std::uint64_t pid) override
    {
        start(pid, collector_scope::process_tree);
    }

    void start(std::uint64_t pid, collector_scope scope) override
    {
        root_pid_ = pid;
        scope_ = scope;
        const auto counters = read_process_tree_counters();
        last_read_ = counters.read_bytes;
        last_write_ = counters.write_bytes;
    }

    std::vector<metric_value> sample() override
    {
        const auto counters = read_process_tree_counters();
        if (!counters.has_value)
        {
            return {
                make_error_metric("file_read_bytes", metric_unit::bytes, "unavailable"),
                make_error_metric("file_write_bytes", metric_unit::bytes, "unavailable"),
            };
        }

        const auto read_delta = counters.read_bytes >= last_read_ ? counters.read_bytes - last_read_ : 0;
        const auto write_delta = counters.write_bytes >= last_write_ ? counters.write_bytes - last_write_ : 0;
        last_read_ = counters.read_bytes;
        last_write_ = counters.write_bytes;

        return {
            make_metric("file_read_bytes", metric_unit::bytes, static_cast<double>(read_delta)),
            make_metric("file_write_bytes", metric_unit::bytes, static_cast<double>(write_delta)),
        };
    }

    void stop() override
    {
    }

    ~file_io_collector() override
    {
        stop();
    }

private:
    struct tree_io_counters
    {
        ULONGLONG read_bytes{0};
        ULONGLONG write_bytes{0};
        bool has_value{false};
    };

    tree_io_counters read_process_tree_counters() const
    {
        tree_io_counters total;
        for (const auto pid : target_pids())
        {
            const HANDLE process_handle = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, static_cast<DWORD>(pid));
            if (process_handle == nullptr)
            {
                continue;
            }

            IO_COUNTERS counters{};
            if (GetProcessIoCounters(process_handle, &counters))
            {
                total.read_bytes += counters.ReadTransferCount;
                total.write_bytes += counters.WriteTransferCount;
                total.has_value = true;
            }
            CloseHandle(process_handle);
        }
        return total;
    }

    std::vector<std::uint64_t> target_pids() const
    {
        if (scope_ == collector_scope::process_only)
        {
            return {root_pid_};
        }
        return enumerate_process_tree(root_pid_);
    }

    std::uint64_t root_pid_{0};
    collector_scope scope_{collector_scope::process_tree};
    ULONGLONG last_read_{0};
    ULONGLONG last_write_{0};
};
}

collector_ptr make_file_io_collector()
{
    return std::make_unique<file_io_collector>();
}
}
