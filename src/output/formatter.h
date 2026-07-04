#pragma once

#include "core/sample.h"

#include <string>
#include <vector>

namespace perfm
{
std::string default_csv_path();
std::string default_markdown_path();
std::string format_metric_value(const metric_value& value);
std::string format_stdout(const std::vector<sample>& samples);
std::string format_csv(const std::vector<sample>& samples);
std::string format_markdown(const std::vector<sample>& samples);
}
