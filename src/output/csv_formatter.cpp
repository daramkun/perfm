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

std::string escape_csv(const std::string& value)
{
    if (value.find_first_of(",\"\n\r") == std::string::npos)
    {
        return value;
    }

    std::string escaped = "\"";
    for (char ch : value)
    {
        if (ch == '"')
        {
            escaped += "\"\"";
        }
        else
        {
            escaped.push_back(ch);
        }
    }
    escaped.push_back('"');
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

std::string default_csv_path()
{
    return "perfm.csv";
}

std::string format_csv(const std::vector<sample>& samples)
{
    const auto columns = collect_columns(samples);
    std::ostringstream output;
    for (std::size_t index = 0; index < columns.size(); ++index)
    {
        if (index > 0)
        {
            output << ',';
        }
        output << escape_csv(columns[index]);
    }
    output << '\n';

    for (const auto& current : samples)
    {
        const auto row = row_values(current);
        for (std::size_t index = 0; index < columns.size(); ++index)
        {
            if (index > 0)
            {
                output << ',';
            }
            output << escape_csv(lookup_cell(row, columns[index]));
        }
        output << '\n';
    }
    return output.str();
}
}
