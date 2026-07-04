#include "collectors/memory_collector.h"

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
        process_handle_ = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_VM_READ, FALSE, static_cast<DWORD>(pid));
    }

    std::vector<metric_value> sample() override
    {
        PROCESS_MEMORY_COUNTERS_EX counters{};
        counters.cb = sizeof(counters);
        if (process_handle_ == nullptr ||
            !GetProcessMemoryInfo(process_handle_, reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&counters), sizeof(counters)))
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
            make_metric("memory_resident_bytes", metric_unit::bytes, static_cast<double>(counters.WorkingSetSize)),
            make_metric("memory_virtual_bytes", metric_unit::bytes, static_cast<double>(counters.PrivateUsage)),
        };
    }

    void stop() override
    {
        if (process_handle_ != nullptr)
        {
            CloseHandle(process_handle_);
            process_handle_ = nullptr;
        }
    }

    ~memory_collector() override
    {
        stop();
    }

private:
    HANDLE process_handle_{nullptr};
};
}

collector_ptr make_memory_collector()
{
    return std::make_unique<memory_collector>();
}
}
