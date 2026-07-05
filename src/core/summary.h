#pragma once

#include "core/sample.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace perfm
{
struct metric_summary
{
    std::uint64_t pid{0};
    std::string name;
    metric_unit unit{metric_unit::none};
    std::size_t count{0};
    double min{0.0};
    double max{0.0};
    double avg{0.0};
    double last{0.0};
    std::size_t unsupported_count{0};
    std::size_t error_count{0};
};

std::vector<metric_summary> summarize_samples(const std::vector<sample>& samples);
}
