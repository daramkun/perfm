#pragma once

#include <chrono>
#include <string>

namespace perfm
{
struct duration_parse_result
{
    bool ok{false};
    std::chrono::milliseconds value{0};
    std::string error;
};

duration_parse_result parse_duration(const std::string& text);
std::chrono::milliseconds default_sample_frequency();
}
