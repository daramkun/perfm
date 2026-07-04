#include "collectors/cpu_collector.h"

#define NOMINMAX
#include <windows.h>

#include <algorithm>
#include <chrono>

namespace perfm
{
namespace
{
unsigned long long file_time_to_uint64(const FILETIME& value)
{
    ULARGE_INTEGER integer{};
    integer.LowPart = value.dwLowDateTime;
    integer.HighPart = value.dwHighDateTime;
    return integer.QuadPart;
}

class cpu_collector final : public collector
{
public:
    void start(std::uint64_t pid) override
    {
        process_handle_ = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, static_cast<DWORD>(pid));
        SYSTEM_INFO info{};
        GetSystemInfo(&info);
        logical_cores_ = std::max<DWORD>(1, info.dwNumberOfProcessors);
        last_wall_ = std::chrono::steady_clock::now();
        last_process_time_ = read_process_time();
    }

    std::vector<metric_value> sample() override
    {
        const auto wall = std::chrono::steady_clock::now();
        const auto process_time = read_process_time();
        const auto wall_delta = std::chrono::duration<double>(wall - last_wall_).count();
        const auto process_delta_seconds = static_cast<double>(process_time - last_process_time_) / 10000000.0;
        double percent = 0.0;
        if (wall_delta > 0.0)
        {
            percent = (process_delta_seconds / wall_delta) * 100.0;
        }

        last_wall_ = wall;
        last_process_time_ = process_time;

        return {
            make_metric("cpu_percent", metric_unit::percent, percent),
            make_metric("cpu_percent_per_logical_core", metric_unit::percent, percent / logical_cores_),
            make_metric("cpu_percent_per_physical_core", metric_unit::percent, percent / logical_cores_),
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

    ~cpu_collector() override
    {
        stop();
    }

private:
    unsigned long long read_process_time() const
    {
        if (process_handle_ == nullptr)
        {
            return 0;
        }

        FILETIME creation{};
        FILETIME exit{};
        FILETIME kernel{};
        FILETIME user{};
        if (!GetProcessTimes(process_handle_, &creation, &exit, &kernel, &user))
        {
            return last_process_time_;
        }
        return file_time_to_uint64(kernel) + file_time_to_uint64(user);
    }

    HANDLE process_handle_{nullptr};
    DWORD logical_cores_{1};
    std::chrono::steady_clock::time_point last_wall_{};
    unsigned long long last_process_time_{0};
};
}

collector_ptr make_cpu_collector()
{
    return std::make_unique<cpu_collector>();
}
}
