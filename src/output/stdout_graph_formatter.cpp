#include "output/formatter.h"

#include <algorithm>
#include <iomanip>
#include <map>
#include <optional>
#include <sstream>

namespace perfm
{
namespace
{
struct graph_series
{
    std::uint64_t pid{0};
    std::string name;
    metric_unit unit{metric_unit::none};
    std::vector<double> values;
    std::size_t unsupported_count{0};
    std::size_t error_count{0};
};

const char* section_name(const std::string& metric_name)
{
    if (metric_name.rfind("cpu_", 0) == 0)
    {
        return "CPU";
    }
    if (metric_name.rfind("memory_", 0) == 0 || metric_name == "gpu_vram_bytes")
    {
        return "Memory";
    }
    if (metric_name.rfind("file_", 0) == 0)
    {
        return "File I/O";
    }
    if (metric_name.rfind("network_", 0) == 0)
    {
        return "Network";
    }
    if (metric_name.rfind("gpu_", 0) == 0)
    {
        return "GPU";
    }
    if (metric_name == "elapsed_time_ms")
    {
        return "Time";
    }
    return "Other";
}

std::string format_scaled_value(double value, metric_unit unit)
{
    if (unit != metric_unit::bytes)
    {
        metric_value metric;
        metric.unit = unit;
        metric.value = value;
        return format_metric_value(metric);
    }

    const char* units[] = {"B", "KB", "MB", "GB", "TB"};
    std::size_t unit_index = 0;
    while (value >= 1024.0 && unit_index + 1 < std::size(units))
    {
        value /= 1024.0;
        ++unit_index;
    }

    std::ostringstream output;
    if (unit_index == 0)
    {
        output << static_cast<long long>(value) << ' ' << units[unit_index];
    }
    else
    {
        output << std::fixed << std::setprecision(1) << value << ' ' << units[unit_index];
    }
    return output.str();
}

std::string sparkline(const std::vector<double>& values)
{
    if (values.empty())
    {
        return {};
    }

    static const char* blocks[] = {"▁", "▂", "▃", "▄", "▅", "▆", "▇", "█"};
    const auto [min_it, max_it] = std::minmax_element(values.begin(), values.end());
    const double min_value = *min_it;
    const double max_value = *max_it;

    std::string output;
    for (const double value : values)
    {
        std::size_t index = 0;
        if (max_value > min_value)
        {
            const auto normalized = (value - min_value) / (max_value - min_value);
            index = static_cast<std::size_t>(std::clamp(normalized, 0.0, 1.0) * 7.0);
        }
        output += blocks[index];
    }
    return output;
}

std::vector<graph_series> collect_series(const std::vector<sample>& samples)
{
    std::map<std::pair<std::uint64_t, std::string>, graph_series> by_metric;
    for (const auto& current : samples)
    {
        for (const auto& value : current.values)
        {
            auto key = std::make_pair(current.pid, value.name);
            auto& series = by_metric[key];
            series.pid = current.pid;
            series.name = value.name;
            series.unit = value.unit;

            if (value.state == metric_state::unsupported)
            {
                ++series.unsupported_count;
                continue;
            }
            if (value.state == metric_state::error)
            {
                ++series.error_count;
                continue;
            }
            series.values.push_back(value.value);
        }
    }

    std::vector<graph_series> series;
    series.reserve(by_metric.size());
    for (auto& item : by_metric)
    {
        series.push_back(std::move(item.second));
    }
    return series;
}

std::size_t max_name_width(const std::vector<graph_series>& series)
{
    std::size_t width = 0;
    for (const auto& item : series)
    {
        width = std::max(width, item.name.size());
    }
    return width;
}

std::optional<std::chrono::milliseconds> last_elapsed(const std::vector<sample>& samples)
{
    if (samples.empty())
    {
        return std::nullopt;
    }
    return samples.back().elapsed;
}
}

std::string format_stdout_graph(const std::vector<sample>& samples)
{
    const auto series = collect_series(samples);
    const auto name_width = max_name_width(series);

    std::ostringstream output;
    output << "elapsed ";
    const auto elapsed = last_elapsed(samples);
    if (elapsed.has_value())
    {
        output << elapsed->count() << " ms";
    }
    else
    {
        output << "0 ms";
    }
    output << "  samples " << samples.size() << "\n";

    const char* current_section = nullptr;
    std::uint64_t current_pid = 0;
    bool wrote_body = false;
    for (const auto& item : series)
    {
        const auto* section = section_name(item.name);
        if (item.pid != current_pid)
        {
            current_pid = item.pid;
            current_section = nullptr;
            if (current_pid != 0)
            {
                output << "\nPID " << current_pid << "\n";
            }
        }
        if (current_section == nullptr || std::string(current_section) != section)
        {
            output << "\n" << section << "\n";
            current_section = section;
        }

        output << std::left << std::setw(static_cast<int>(name_width)) << item.name << "  ";
        if (item.values.empty())
        {
            if (item.unsupported_count > 0)
            {
                output << "unsupported";
            }
            else if (item.error_count > 0)
            {
                output << "error";
            }
            else
            {
                output << "no data";
            }
            output << '\n';
            wrote_body = true;
            continue;
        }

        const auto [min_it, max_it] = std::minmax_element(item.values.begin(), item.values.end());
        const auto last = item.values.back();
        output << sparkline(item.values);
        output << "  last " << std::setw(10) << format_scaled_value(last, item.unit);
        output << "  max " << std::setw(10) << format_scaled_value(*max_it, item.unit);
        output << "  min " << format_scaled_value(*min_it, item.unit);
        if (item.unsupported_count > 0 || item.error_count > 0)
        {
            output << "  unsupported " << item.unsupported_count << "  errors " << item.error_count;
        }
        output << '\n';
        wrote_body = true;
    }

    if (!wrote_body)
    {
        output << "\nNo graphable metrics.\n";
    }
    return output.str();
}
}
