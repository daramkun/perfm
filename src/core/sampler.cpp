#include "core/sampler.h"
#include "process/process_tree.h"

#include <algorithm>
#include <map>
#include <thread>

namespace perfm
{
namespace
{
sample collect_sample(collector_list& collectors, bool include_elapsed_time, std::chrono::steady_clock::time_point start)
{
    const auto now = std::chrono::steady_clock::now();
    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - start);
    sample current;
    current.elapsed = elapsed;

    if (include_elapsed_time)
    {
        current.values.push_back(make_metric("elapsed_time_ms", metric_unit::milliseconds, static_cast<double>(elapsed.count())));
    }

    for (auto& item : collectors)
    {
        auto values = item->sample();
        current.values.insert(current.values.end(), std::make_move_iterator(values.begin()), std::make_move_iterator(values.end()));
    }

    return current;
}

sample collect_process_sample(std::uint64_t pid,
                              collector_list& collectors,
                              bool include_elapsed_time,
                              std::chrono::steady_clock::time_point start)
{
    auto current = collect_sample(collectors, include_elapsed_time, start);
    current.pid = pid;
    return current;
}

void sleep_until_or_process_exit(child_process& process, std::chrono::steady_clock::time_point deadline)
{
    while (process.is_running())
    {
        const auto now = std::chrono::steady_clock::now();
        if (now >= deadline)
        {
            break;
        }
        std::this_thread::sleep_until(std::min(deadline, now + std::chrono::milliseconds(25)));
    }
}

std::vector<sample> sample_process_split(child_process& process, const sampler_config& config)
{
    const auto start = std::chrono::steady_clock::now();
    const auto root_pid = process.pid();
    std::map<std::uint64_t, collector_list> collectors_by_pid;
    std::vector<sample> samples;

    auto ensure_collectors = [&](std::uint64_t pid) -> collector_list& {
        auto found = collectors_by_pid.find(pid);
        if (found == collectors_by_pid.end())
        {
            auto collectors = config.collector_factory ? config.collector_factory() : collector_list{};
            for (auto& item : collectors)
            {
                item->start(pid, collector_scope::process_only);
            }
            found = collectors_by_pid.emplace(pid, std::move(collectors)).first;
        }
        return found->second;
    };

    if (!config.collector_factory)
    {
        process.wait();
        if (config.include_elapsed_time)
        {
            sample current;
            current.pid = root_pid;
            current.elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start);
            current.values.push_back(make_metric("elapsed_time_ms", metric_unit::milliseconds, static_cast<double>(current.elapsed.count())));
            samples.push_back(std::move(current));
        }
        return samples;
    }

    auto next_sample = start;
    while (process.is_running())
    {
        for (const auto pid : enumerate_process_tree(root_pid))
        {
            samples.push_back(collect_process_sample(pid, ensure_collectors(pid), false, start));
        }
        next_sample += config.frequency;
        sleep_until_or_process_exit(process, next_sample);
    }

    if (config.include_elapsed_time)
    {
        samples.push_back(collect_process_sample(root_pid, ensure_collectors(root_pid), true, start));
    }

    process.wait();

    for (auto& item : collectors_by_pid)
    {
        for (auto& collector : item.second)
        {
            collector->stop();
        }
    }

    return samples;
}
}

std::vector<sample> sample_process(child_process& process, collector_list& collectors, const sampler_config& config)
{
    const auto start = std::chrono::steady_clock::now();
    for (auto& item : collectors)
    {
        item->start(process.pid(), collector_scope::process_tree);
    }

    if (config.split_subprocesses && !collectors.empty())
    {
        for (auto& item : collectors)
        {
            item->stop();
        }
        return sample_process_split(process, config);
    }

    std::vector<sample> samples;
    if (collectors.empty())
    {
        process.wait();
        if (config.include_elapsed_time)
        {
            samples.push_back(collect_sample(collectors, true, start));
        }
        return samples;
    }

    auto next_sample = start;
    while (process.is_running())
    {
        samples.push_back(collect_sample(collectors, false, start));
        next_sample += config.frequency;
        sleep_until_or_process_exit(process, next_sample);
    }

    const auto final_elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start);
    if (config.include_elapsed_time || samples.empty() ||
        final_elapsed - samples.back().elapsed >= std::min(config.frequency, std::chrono::milliseconds(100)))
    {
        samples.push_back(collect_sample(collectors, config.include_elapsed_time, start));
    }

    process.wait();

    for (auto& item : collectors)
    {
        item->stop();
    }

    return samples;
}
}
