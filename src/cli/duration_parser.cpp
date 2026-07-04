#include "cli/duration_parser.h"

#include <charconv>

namespace perfm
{
std::chrono::milliseconds default_sample_frequency()
{
    return std::chrono::seconds(5);
}

duration_parse_result parse_duration(const std::string& text)
{
    if (text.empty())
    {
        return {false, {}, "duration is empty"};
    }

    std::size_t unit_start = 0;
    while (unit_start < text.size() && text[unit_start] >= '0' && text[unit_start] <= '9')
    {
        ++unit_start;
    }

    if (unit_start == 0 || unit_start == text.size())
    {
        return {false, {}, "duration must be a positive integer followed by ms, s, m, or h"};
    }

    long long amount = 0;
    const auto number_text = text.substr(0, unit_start);
    const auto [ptr, ec] = std::from_chars(number_text.data(), number_text.data() + number_text.size(), amount);
    if (ec != std::errc() || ptr != number_text.data() + number_text.size() || amount <= 0)
    {
        return {false, {}, "duration must be greater than zero"};
    }

    const auto unit = text.substr(unit_start);
    if (unit == "ms")
    {
        return {true, std::chrono::milliseconds(amount), {}};
    }
    if (unit == "s")
    {
        return {true, std::chrono::seconds(amount), {}};
    }
    if (unit == "m")
    {
        return {true, std::chrono::minutes(amount), {}};
    }
    if (unit == "h")
    {
        return {true, std::chrono::hours(amount), {}};
    }

    return {false, {}, "duration unit must be ms, s, m, or h"};
}
}
