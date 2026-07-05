#include "collectors/cpu_collector.h"
#include "process/process_tree.h"

#include <libproc.h>
#include <sys/sysctl.h>

#include <algorithm>
#include <chrono>

namespace perfm
{
namespace
{
int read_sysctl_int(const char* name, int fallback)
{
    int value = fallback;
    std::size_t size = sizeof(value);
    if (sysctlbyname(name, &value, &size, nullptr, 0) != 0 || value <= 0)
    {
        return fallback;
    }
    return value;
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
        logical_cores_ = read_sysctl_int("hw.logicalcpu", 1);
        physical_cores_ = read_sysctl_int("hw.physicalcpu", logical_cores_);
        last_wall_ = std::chrono::steady_clock::now();
        last_process_time_ns_ = read_process_tree_time_ns();
    }

    std::vector<metric_value> sample() override
    {
        const auto wall = std::chrono::steady_clock::now();
        const auto process_time_ns = read_process_tree_time_ns();
        const auto wall_delta = std::chrono::duration<double>(wall - last_wall_).count();
        const auto process_delta_ns =
            process_time_ns >= last_process_time_ns_ ? process_time_ns - last_process_time_ns_ : 0;
        const auto process_delta_seconds = static_cast<double>(process_delta_ns) / 1000000000.0;
        double percent = 0.0;
        if (wall_delta > 0.0)
        {
            percent = (process_delta_seconds / wall_delta) * 100.0;
        }

        last_wall_ = wall;
        last_process_time_ns_ = process_time_ns;

        return {
            make_metric("cpu_percent", metric_unit::percent, percent),
            make_metric("cpu_percent_per_logical_core", metric_unit::percent, percent / std::max(1, logical_cores_)),
            make_metric("cpu_percent_per_physical_core", metric_unit::percent, percent / std::max(1, physical_cores_)),
        };
    }

    void stop() override
    {
    }

private:
    unsigned long long read_process_time_ns(std::uint64_t pid) const
    {
        rusage_info_v4 info{};
        if (proc_pid_rusage(static_cast<int>(pid), RUSAGE_INFO_V4, reinterpret_cast<rusage_info_t*>(&info)) != 0)
        {
            return 0;
        }
        return info.ri_user_time + info.ri_system_time;
    }

    unsigned long long read_process_tree_time_ns() const
    {
        unsigned long long total = 0;
        for (const auto pid : target_pids())
        {
            total += read_process_time_ns(pid);
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
    int logical_cores_{1};
    int physical_cores_{1};
    std::chrono::steady_clock::time_point last_wall_{};
    unsigned long long last_process_time_ns_{0};
};
}

collector_ptr make_cpu_collector()
{
    return std::make_unique<cpu_collector>();
}
}
