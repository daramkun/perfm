#pragma once

#include "cli/options.h"

#include <string>
#include <vector>

namespace perfm
{
struct parse_result
{
    bool ok{false};
    options value;
    std::string error;
};

parse_result parse_options(const std::vector<std::string>& arguments);
std::string format_usage();
}
