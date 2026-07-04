#include "output/formatter.h"

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

std::string escape_markdown(const std::string& value)
{
    std::string escaped;
    for (char ch : value)
    {
        if (ch == '|')
        {
            escaped += "\\|";
        }
        else
        {
            escaped.push_back(ch);
        }
    }
    return escaped;
}

std::map<std::string, std::string> row_values(const sample& current)
{
    std::map<std::string, std::string> values;
    values["sample_ms"] = std::to_string(current.elapsed.count());
    for (const auto& value : current.values)
    {
        values[value.name] = format_metric_value(value);
    }
    return values;
}

std::string lookup_cell(const std::map<std::string, std::string>& row, const std::string& column)
{
    const auto found = row.find(column);
    if (found == row.end())
    {
        return {};
    }
    return found->second;
}
}

std::string default_markdown_path()
{
    return "perfm.md";
}

std::string format_markdown(const std::vector<sample>& samples)
{
    const auto columns = collect_columns(samples);
    std::ostringstream output;
    output << "|";
    for (const auto& column : columns)
    {
        output << " " << escape_markdown(column) << " |";
    }
    output << '\n';

    output << "|";
    for (std::size_t index = 0; index < columns.size(); ++index)
    {
        output << " --- |";
    }
    output << '\n';

    for (const auto& current : samples)
    {
        const auto row = row_values(current);
        output << "|";
        for (const auto& column : columns)
        {
            output << " " << escape_markdown(lookup_cell(row, column)) << " |";
        }
        output << '\n';
    }

    return output.str();
}
}
