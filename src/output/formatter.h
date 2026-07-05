#pragma once

#include "core/sample.h"
#include "core/summary.h"

#include <string>
#include <vector>

namespace perfm
{
std::string default_csv_path();
std::string default_markdown_path();
std::string default_json_path();
std::string format_metric_value(const metric_value& value);
std::string format_stdout(const std::vector<sample>& samples);
std::string format_stdout_graph(const std::vector<sample>& samples);
std::string format_csv(const std::vector<sample>& samples);
std::string format_markdown(const std::vector<sample>& samples);
std::string format_json(const std::vector<sample>& samples);
std::string format_stdout_summary(const std::vector<metric_summary>& summaries);
std::string format_csv_summary(const std::vector<metric_summary>& summaries);
std::string format_markdown_summary(const std::vector<metric_summary>& summaries);
std::string format_json_summary(const std::vector<metric_summary>& summaries);
}
