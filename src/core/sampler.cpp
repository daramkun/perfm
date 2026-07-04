#include "core/sampler.h"

#include <algorithm>
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
}

std::vector<sample> sample_process(child_process& process, collector_list& collectors, const sampler_config& config)
{
    const auto start = std::chrono::steady_clock::now();
    for (auto& item : collectors)
    {
        item->start(process.pid());
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
        std::this_thread::sleep_until(next_sample);
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
