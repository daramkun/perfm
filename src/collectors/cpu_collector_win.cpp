#include "collectors/cpu_collector.h"
#include "process/process_tree.h"

#define NOMINMAX
#include <windows.h>

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
        start(pid, collector_scope::process_tree);
    }

    void start(std::uint64_t pid, collector_scope scope) override
    {
        root_pid_ = pid;
        scope_ = scope;
        last_wall_ = std::chrono::steady_clock::now();
        last_process_time_ = read_process_tree_time();
    }

    std::vector<metric_value> sample() override
    {
        const auto wall = std::chrono::steady_clock::now();
        const auto process_time = read_process_tree_time();
        const auto wall_delta = std::chrono::duration<double>(wall - last_wall_).count();
        const auto process_delta = process_time >= last_process_time_ ? process_time - last_process_time_ : 0;
        const auto process_delta_seconds = static_cast<double>(process_delta) / 10000000.0;
        double percent = 0.0;
        if (wall_delta > 0.0)
        {
            percent = (process_delta_seconds / wall_delta) * 100.0;
        }

        last_wall_ = wall;
        last_process_time_ = process_time;

        return {
            make_metric("cpu_percent", metric_unit::percent, percent),
            make_metric("cpu_total_percent", metric_unit::percent, percent),
        };
    }

    void stop() override
    {
    }

    ~cpu_collector() override
    {
        stop();
    }

private:
    unsigned long long read_process_time(std::uint64_t pid) const
    {
        const HANDLE process_handle = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, static_cast<DWORD>(pid));
        if (process_handle == nullptr)
        {
            return 0;
        }

        FILETIME creation{};
        FILETIME exit{};
        FILETIME kernel{};
        FILETIME user{};
        if (!GetProcessTimes(process_handle, &creation, &exit, &kernel, &user))
        {
            CloseHandle(process_handle);
            return 0;
        }
        const auto process_time = file_time_to_uint64(kernel) + file_time_to_uint64(user);
        CloseHandle(process_handle);
        return process_time;
    }

    unsigned long long read_process_tree_time() const
    {
        unsigned long long total = 0;
        for (const auto pid : target_pids())
        {
            total += read_process_time(pid);
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
    std::chrono::steady_clock::time_point last_wall_{};
    unsigned long long last_process_time_{0};
};
}

collector_ptr make_cpu_collector()
{
    return std::make_unique<cpu_collector>();
}
}
