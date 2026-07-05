#include "output/formatter.h"

#include <algorithm>
#include <iomanip>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace perfm
{
namespace
{
std::string format_summary_number(double value, metric_unit unit)
{
    metric_value metric;
    metric.unit = unit;
    metric.value = value;
    return format_metric_value(metric);
}

std::vector<std::string> summary_columns(const std::vector<metric_summary>& summaries)
{
    std::vector<std::string> columns;
    bool has_pid = false;
    for (const auto& summary : summaries)
    {
        if (summary.pid != 0)
        {
            has_pid = true;
            break;
        }
    }
    if (has_pid)
    {
        columns.push_back("pid");
    }
    columns.push_back("metric");
    columns.push_back("count");
    columns.push_back("min");
    columns.push_back("max");
    columns.push_back("avg");
    columns.push_back("last");
    columns.push_back("unsupported");
    columns.push_back("errors");
    return columns;
}

std::map<std::string, std::string> summary_row(const metric_summary& summary)
{
    std::map<std::string, std::string> values;
    if (summary.pid != 0)
    {
        values["pid"] = std::to_string(summary.pid);
    }
    values["metric"] = summary.name;
    values["count"] = std::to_string(summary.count);
    if (summary.count > 0)
    {
        values["min"] = format_summary_number(summary.min, summary.unit);
        values["max"] = format_summary_number(summary.max, summary.unit);
        values["avg"] = format_summary_number(summary.avg, summary.unit);
        values["last"] = format_summary_number(summary.last, summary.unit);
    }
    values["unsupported"] = std::to_string(summary.unsupported_count);
    values["errors"] = std::to_string(summary.error_count);
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

std::string escape_csv(const std::string& value)
{
    if (value.find_first_of(",\"\n\r") == std::string::npos)
    {
        return value;
    }

    std::string escaped = "\"";
    for (const char ch : value)
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

std::string escape_markdown(const std::string& value)
{
    std::string escaped;
    for (const char ch : value)
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

std::string escape_json(const std::string& value)
{
    std::ostringstream output;
    for (const char ch : value)
    {
        switch (ch)
        {
        case '\\':
            output << "\\\\";
            break;
        case '"':
            output << "\\\"";
            break;
        case '\n':
            output << "\\n";
            break;
        case '\r':
            output << "\\r";
            break;
        case '\t':
            output << "\\t";
            break;
        default:
            output << ch;
            break;
        }
    }
    return output.str();
}
}

std::string format_stdout_summary(const std::vector<metric_summary>& summaries)
{
    const auto columns = summary_columns(summaries);
    std::vector<std::size_t> widths;
    widths.reserve(columns.size());
    for (const auto& column : columns)
    {
        widths.push_back(column.size());
    }

    std::vector<std::map<std::string, std::string>> rows;
    for (const auto& summary : summaries)
    {
        rows.push_back(summary_row(summary));
        for (std::size_t index = 0; index < columns.size(); ++index)
        {
            widths[index] = std::max(widths[index], lookup_cell(rows.back(), columns[index]).size());
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
            output << std::left << std::setw(static_cast<int>(widths[index])) << lookup_cell(row, columns[index]);
        }
        output << '\n';
    }
    return output.str();
}

std::string format_csv_summary(const std::vector<metric_summary>& summaries)
{
    const auto columns = summary_columns(summaries);
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

    for (const auto& summary : summaries)
    {
        const auto row = summary_row(summary);
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

std::string format_markdown_summary(const std::vector<metric_summary>& summaries)
{
    const auto columns = summary_columns(summaries);
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

    for (const auto& summary : summaries)
    {
        const auto row = summary_row(summary);
        output << "|";
        for (const auto& column : columns)
        {
            output << " " << escape_markdown(lookup_cell(row, column)) << " |";
        }
        output << '\n';
    }
    return output.str();
}

std::string format_json_summary(const std::vector<metric_summary>& summaries)
{
    std::ostringstream output;
    output << std::setprecision(15);
    output << "{\n  \"summary\": [\n";
    for (std::size_t index = 0; index < summaries.size(); ++index)
    {
        const auto& summary = summaries[index];
        output << "    {\n";
        if (summary.pid != 0)
        {
            output << "      \"pid\": " << summary.pid << ",\n";
        }
        output << "      \"metric\": \"" << escape_json(summary.name) << "\",\n";
        output << "      \"count\": " << summary.count << ",\n";
        if (summary.count > 0)
        {
            output << "      \"min\": " << summary.min << ",\n";
            output << "      \"max\": " << summary.max << ",\n";
            output << "      \"avg\": " << summary.avg << ",\n";
            output << "      \"last\": " << summary.last << ",\n";
        }
        output << "      \"unsupported\": " << summary.unsupported_count << ",\n";
        output << "      \"errors\": " << summary.error_count << "\n";
        output << "    }";
        if (index + 1 < summaries.size())
        {
            output << ",";
        }
        output << '\n';
    }
    output << "  ]\n}\n";
    return output.str();
}
}
