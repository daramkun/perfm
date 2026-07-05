#include "collectors/cpu_collector.h"
#include "process/process_tree.h"

#include <dirent.h>

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <utility>
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
        last_thread_ticks_ = read_process_tree_thread_ticks();
    }

    std::vector<metric_value> sample() override
    {
        const auto wall = std::chrono::steady_clock::now();
        const auto thread_ticks = read_process_tree_thread_ticks();
        const auto wall_delta = std::chrono::duration<double>(wall - last_wall_).count();
        std::vector<unsigned long long> core_deltas(static_cast<std::size_t>(std::max<long>(1, logical_cores_)), 0);
        unsigned long long total_tick_delta = 0;
        for (const auto& item : thread_ticks)
        {
            const auto found = last_thread_ticks_.find(item.first);
            if (found == last_thread_ticks_.end() || item.second.ticks < found->second.ticks)
            {
                continue;
            }

            const auto tick_delta = item.second.ticks - found->second.ticks;
            total_tick_delta += tick_delta;
            if (item.second.processor >= 0 && item.second.processor < static_cast<int>(core_deltas.size()))
            {
                core_deltas[static_cast<std::size_t>(item.second.processor)] += tick_delta;
            }
        }

        const auto process_delta = static_cast<double>(total_tick_delta) / static_cast<double>(clock_ticks_);
        double percent = 0.0;
        if (wall_delta > 0.0)
        {
            percent = (process_delta / wall_delta) * 100.0;
        }

        last_wall_ = wall;
        last_thread_ticks_ = thread_ticks;

        std::vector<metric_value> values{
            make_metric("cpu_percent", metric_unit::percent, percent),
            make_metric("cpu_total_percent", metric_unit::percent, percent),
        };
        for (std::size_t index = 0; index < core_deltas.size(); ++index)
        {
            double core_percent = 0.0;
            if (wall_delta > 0.0)
            {
                core_percent = (static_cast<double>(core_deltas[index]) / static_cast<double>(clock_ticks_) / wall_delta) * 100.0;
            }
            values.push_back(make_metric("cpu_" + std::to_string(index + 1) + "_usage", metric_unit::percent, core_percent));
        }
        return values;
    }

    void stop() override
    {
    }

private:
    struct thread_sample
    {
        unsigned long long ticks{0};
        int processor{-1};
    };

    using thread_key = std::pair<std::uint64_t, std::uint64_t>;

    static bool parse_stat_line(const std::string& line, thread_sample& sample)
    {
        const auto closing_paren = line.rfind(')');
        if (closing_paren == std::string::npos)
        {
            return false;
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
            }
            else if (index == 39)
            {
                sample.processor = std::stoi(field);
                break;
            }
        }
        sample.ticks = user_ticks + kernel_ticks;
        return true;
    }

    static bool read_thread_stat(std::uint64_t pid, std::uint64_t tid, thread_sample& sample)
    {
        std::ifstream input("/proc/" + std::to_string(pid) + "/task/" + std::to_string(tid) + "/stat");
        std::string line;
        std::getline(input, line);
        if (line.empty())
        {
            return false;
        }
        try
        {
            return parse_stat_line(line, sample);
        }
        catch (...)
        {
            return false;
        }
    }

    std::map<thread_key, thread_sample> read_process_tree_thread_ticks() const
    {
        std::map<thread_key, thread_sample> result;
        for (const auto pid : target_pids())
        {
            DIR* tasks = opendir(("/proc/" + std::to_string(pid) + "/task").c_str());
            if (tasks == nullptr)
            {
                continue;
            }

            while (const auto* entry = readdir(tasks))
            {
                if (entry->d_name[0] < '0' || entry->d_name[0] > '9')
                {
                    continue;
                }
                const auto tid = static_cast<std::uint64_t>(std::strtoull(entry->d_name, nullptr, 10));
                thread_sample sample;
                if (tid != 0 && read_thread_stat(pid, tid, sample))
                {
                    result[{pid, tid}] = sample;
                }
            }
            closedir(tasks);
        }
        return result;
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
    std::map<thread_key, thread_sample> last_thread_ticks_;
};
}

collector_ptr make_cpu_collector()
{
    return std::make_unique<cpu_collector>();
}
}
