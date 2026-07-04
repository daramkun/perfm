#include "collectors/cpu_collector.h"
#include "process/process_tree.h"

#include <chrono>
#include <fstream>
#include <sstream>
#include <string>
#include <unistd.h>

namespace perfm
{
namespace
{
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
        clock_ticks_ = sysconf(_SC_CLK_TCK);
        logical_cores_ = sysconf(_SC_NPROCESSORS_ONLN);
        last_wall_ = std::chrono::steady_clock::now();
        last_ticks_ = read_process_tree_ticks();
    }

    std::vector<metric_value> sample() override
    {
        const auto wall = std::chrono::steady_clock::now();
        const auto ticks = read_process_tree_ticks();
        const auto wall_delta = std::chrono::duration<double>(wall - last_wall_).count();
        const auto tick_delta = ticks >= last_ticks_ ? ticks - last_ticks_ : 0;
        const auto process_delta = static_cast<double>(tick_delta) / static_cast<double>(clock_ticks_);
        double percent = 0.0;
        if (wall_delta > 0.0)
        {
            percent = (process_delta / wall_delta) * 100.0;
        }

        last_wall_ = wall;
        last_ticks_ = ticks;

        const auto cores = logical_cores_ <= 0 ? 1.0 : static_cast<double>(logical_cores_);
        return {
            make_metric("cpu_percent", metric_unit::percent, percent),
            make_metric("cpu_percent_per_logical_core", metric_unit::percent, percent / cores),
            make_metric("cpu_percent_per_physical_core", metric_unit::percent, percent / cores),
        };
    }

    void stop() override
    {
    }

private:
    unsigned long long read_process_ticks(std::uint64_t pid) const
    {
        std::ifstream input("/proc/" + std::to_string(pid) + "/stat");
        std::string line;
        std::getline(input, line);
        const auto closing_paren = line.rfind(')');
        if (closing_paren == std::string::npos)
        {
            return 0;
        }

        std::istringstream fields(line.substr(closing_paren + 2));
        std::string field;
        unsigned long long user_ticks = 0;
        unsigned long long kernel_ticks = 0;
        for (int index = 3; fields >> field; ++index)
        {
            if (index == 14)
            {
                user_ticks = std::stoull(field);
            }
            else if (index == 15)
            {
                kernel_ticks = std::stoull(field);
                break;
            }
        }
        return user_ticks + kernel_ticks;
    }

    unsigned long long read_process_tree_ticks() const
    {
        unsigned long long total = 0;
        for (const auto pid : target_pids())
        {
            total += read_process_ticks(pid);
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
    long clock_ticks_{100};
    long logical_cores_{1};
    std::chrono::steady_clock::time_point last_wall_{};
    unsigned long long last_ticks_{0};
};
}

collector_ptr make_cpu_collector()
{
    return std::make_unique<cpu_collector>();
}
}
