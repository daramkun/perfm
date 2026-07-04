#include "collectors/memory_collector.h"
#include "process/process_tree.h"

#define NOMINMAX
#include <windows.h>
#include <psapi.h>

namespace perfm
{
namespace
{
class memory_collector final : public collector
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
    }

    std::vector<metric_value> sample() override
    {
        unsigned long long resident_bytes = 0;
        unsigned long long virtual_bytes = 0;
        bool has_value = false;
        for (const auto pid : target_pids())
        {
            PROCESS_MEMORY_COUNTERS_EX counters{};
            counters.cb = sizeof(counters);
            const HANDLE process_handle = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_VM_READ, FALSE, static_cast<DWORD>(pid));
            if (process_handle == nullptr)
            {
                continue;
            }
            if (GetProcessMemoryInfo(process_handle, reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&counters), sizeof(counters)))
            {
                resident_bytes += counters.WorkingSetSize;
                virtual_bytes += counters.PrivateUsage;
                has_value = true;
            }
            CloseHandle(process_handle);
        }

        if (!has_value)
        {
            return {
                make_error_metric("memory_total_bytes", metric_unit::bytes, "unavailable"),
                make_error_metric("memory_resident_bytes", metric_unit::bytes, "unavailable"),
                make_error_metric("memory_virtual_bytes", metric_unit::bytes, "unavailable"),
            };
        }

        MEMORYSTATUSEX memory_status{};
        memory_status.dwLength = sizeof(memory_status);
        GlobalMemoryStatusEx(&memory_status);

        return {
            make_metric("memory_total_bytes", metric_unit::bytes, static_cast<double>(memory_status.ullTotalPhys)),
            make_metric("memory_resident_bytes", metric_unit::bytes, static_cast<double>(resident_bytes)),
            make_metric("memory_virtual_bytes", metric_unit::bytes, static_cast<double>(virtual_bytes)),
        };
    }

    void stop() override
    {
    }

    ~memory_collector() override
    {
        stop();
    }

private:
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
};
}

collector_ptr make_memory_collector()
{
    return std::make_unique<memory_collector>();
}
}
