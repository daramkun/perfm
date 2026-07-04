#pragma once

#include "core/metric.h"

#include <chrono>
#include <optional>
#include <string>
#include <vector>

namespace perfm
{
enum class output_mode
{
    stdout_table,
    csv,
    markdown
};

struct options
{
    output_mode mode{output_mode::stdout_table};
    std::optional<std::string> output_path;
    std::vector<metric_kind> metrics;
    std::chrono::milliseconds sample_frequency{std::chrono::seconds(5)};
    bool split_subprocesses{false};
    std::string target_path;
    std::vector<std::string> target_args;
};
}
