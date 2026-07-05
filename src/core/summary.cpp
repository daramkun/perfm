#include "core/summary.h"

#include <algorithm>
#include <map>
#include <utility>

namespace perfm
{
namespace
{
struct summary_accumulator
{
    metric_summary summary;
    double total{0.0};
    bool has_value{false};
};
}

std::vector<metric_summary> summarize_samples(const std::vector<sample>& samples)
{
    std::map<std::pair<std::uint64_t, std::string>, summary_accumulator> by_metric;

    for (const auto& current : samples)
    {
        for (const auto& value : current.values)
        {
            auto key = std::make_pair(current.pid, value.name);
            auto& item = by_metric[key];
            item.summary.pid = current.pid;
            item.summary.name = value.name;
            item.summary.unit = value.unit;

            if (value.state == metric_state::unsupported)
            {
                ++item.summary.unsupported_count;
                continue;
            }
            if (value.state == metric_state::error)
            {
                ++item.summary.error_count;
                continue;
            }

            if (!item.has_value)
            {
                item.summary.min = value.value;
                item.summary.max = value.value;
                item.has_value = true;
            }
            else
            {
                item.summary.min = std::min(item.summary.min, value.value);
                item.summary.max = std::max(item.summary.max, value.value);
            }

            item.summary.last = value.value;
            item.total += value.value;
            ++item.summary.count;
        }
    }

    std::vector<metric_summary> summaries;
    summaries.reserve(by_metric.size());
    for (auto& item : by_metric)
    {
        if (item.second.summary.count > 0)
        {
            item.second.summary.avg = item.second.total / static_cast<double>(item.second.summary.count);
        }
        summaries.push_back(std::move(item.second.summary));
    }
    return summaries;
}
}
