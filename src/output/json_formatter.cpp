#include "output/formatter.h"

#include <iomanip>
#include <sstream>

namespace perfm
{
namespace
{
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
        case '\b':
            output << "\\b";
            break;
        case '\f':
            output << "\\f";
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
            if (static_cast<unsigned char>(ch) < 0x20)
            {
                output << "\\u" << std::hex << std::setw(4) << std::setfill('0') << static_cast<int>(static_cast<unsigned char>(ch));
            }
            else
            {
                output << ch;
            }
            break;
        }
    }
    return output.str();
}

const char* state_name(metric_state state)
{
    switch (state)
    {
    case metric_state::ok:
        return "ok";
    case metric_state::unsupported:
        return "unsupported";
    case metric_state::error:
        return "error";
    }
    return "error";
}

void write_metric_json(std::ostringstream& output, const metric_value& value)
{
    if (value.state == metric_state::ok)
    {
        output << value.value;
        return;
    }

    output << "{\"state\":\"" << state_name(value.state) << "\"";
    if (!value.message.empty())
    {
        output << ",\"message\":\"" << escape_json(value.message) << "\"";
    }
    output << "}";
}
}

std::string default_json_path()
{
    return "perfm.json";
}

std::string format_json(const std::vector<sample>& samples)
{
    std::ostringstream output;
    output << std::setprecision(15);
    output << "{\n  \"samples\": [\n";

    for (std::size_t sample_index = 0; sample_index < samples.size(); ++sample_index)
    {
        const auto& current = samples[sample_index];
        output << "    {\n";
        output << "      \"sample_ms\": " << current.elapsed.count();
        if (current.pid != 0)
        {
            output << ",\n      \"pid\": " << current.pid;
        }
        output << ",\n      \"metrics\": {";

        for (std::size_t metric_index = 0; metric_index < current.values.size(); ++metric_index)
        {
            const auto& value = current.values[metric_index];
            output << "\n        \"" << escape_json(value.name) << "\": ";
            write_metric_json(output, value);
            if (metric_index + 1 < current.values.size())
            {
                output << ",";
            }
        }
        if (!current.values.empty())
        {
            output << "\n      ";
        }
        output << "}\n    }";
        if (sample_index + 1 < samples.size())
        {
            output << ",";
        }
        output << "\n";
    }

    output << "  ]\n}\n";
    return output.str();
}
}
