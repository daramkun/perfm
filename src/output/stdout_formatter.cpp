#include "output/formatter.h"

#include <iomanip>
#include <map>
#include <set>
#include <sstream>

namespace perfm
{
namespace
{
std::vector<std::string> collect_columns(const std::vector<sample>& samples)
{
    std::vector<std::string> columns{"sample_ms"};
    for (const auto& current : samples)
    {
        if (current.pid != 0)
        {
            columns.push_back("pid");
            break;
        }
    }
    std::set<std::string> seen;
    for (const auto& current : samples)
    {
        for (const auto& value : current.values)
        {
            if (seen.insert(value.name).second)
            {
                columns.push_back(value.name);
            }
        }
    }
    return columns;
}

std::map<std::string, std::string> row_values(const sample& current)
{
    std::map<std::string, std::string> values;
    values["sample_ms"] = std::to_string(current.elapsed.count());
    if (current.pid != 0)
    {
        values["pid"] = std::to_string(current.pid);
    }
    for (const auto& value : current.values)
    {
        values[value.name] = format_metric_value(value);
    }
    return values;
}
}

std::string format_metric_value(const metric_value& value)
{
    if (value.state == metric_state::unsupported)
    {
        return "unsupported";
    }
    if (value.state == metric_state::error)
    {
        return "error:" + value.message;
    }

    std::ostringstream output;
    if (value.unit == metric_unit::bytes || value.unit == metric_unit::milliseconds)
    {
        output << static_cast<long long>(value.value);
    }
    else
    {
        output << std::fixed << std::setprecision(2) << value.value;
    }
    return output.str();
}

std::string format_stdout(const std::vector<sample>& samples)
{
    const auto columns = collect_columns(samples);
    std::vector<std::size_t> widths;
    widths.reserve(columns.size());
    for (const auto& column : columns)
    {
        widths.push_back(column.size());
    }

    std::vector<std::map<std::string, std::string>> rows;
    for (const auto& current : samples)
    {
        rows.push_back(row_values(current));
        for (std::size_t index = 0; index < columns.size(); ++index)
        {
            widths[index] = std::max(widths[index], rows.back()[columns[index]].size());
        }
    }

    std::ostringstream output;
    for (std::size_t index = 0; index < columns.size(); ++index)
    {
        if (index > 0)
        {
            output << "  ";
        }
        output << std::left << std::setw(static_cast<int>(widths[index])) << columns[index];
    }
    output << '\n';

    for (const auto& row : rows)
    {
        for (std::size_t index = 0; index < columns.size(); ++index)
        {
            if (index > 0)
            {
                output << "  ";
            }
            output << std::left << std::setw(static_cast<int>(widths[index])) << row.at(columns[index]);
        }
        output << '\n';
    }

    return output.str();
}
}
