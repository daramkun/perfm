#include "output/formatter.h"

#include <cassert>
#include <chrono>
#include <string>
#include <vector>

namespace
{
std::vector<perfm::sample> make_samples()
{
    perfm::sample first;
    first.elapsed = std::chrono::milliseconds(0);
    first.values.push_back(perfm::make_metric("elapsed_time_ms", perfm::metric_unit::milliseconds, 0));
    first.values.push_back(perfm::make_unsupported_metric("gpu_percent", perfm::metric_unit::percent));

    perfm::sample second;
    second.elapsed = std::chrono::milliseconds(100);
    second.values.push_back(perfm::make_metric("elapsed_time_ms", perfm::metric_unit::milliseconds, 100));
    second.values.push_back(perfm::make_metric("cpu_percent", perfm::metric_unit::percent, 12.5));
    second.values.push_back(perfm::make_metric("cpu_total_percent", perfm::metric_unit::percent, 12.5));
    second.values.push_back(perfm::make_metric("memory_resident_bytes", perfm::metric_unit::bytes, 1024 * 1024));

    return {first, second};
}

void formatter_renders_stdout_columns_and_unsupported_values()
{
    const auto text = perfm::format_stdout(make_samples());
    assert(text.find("sample_ms") != std::string::npos);
    assert(text.find("elapsed_time_ms") != std::string::npos);
    assert(text.find("unsupported") != std::string::npos);
}

void formatter_renders_csv_header_and_rows()
{
    const auto text = perfm::format_csv(make_samples());
    assert(text.find("sample_ms,elapsed_time_ms,gpu_percent,cpu_percent,cpu_total_percent,memory_resident_bytes\n") == 0);
    assert(text.find("unsupported") != std::string::npos);
    assert(perfm::default_csv_path() == "perfm.csv");
}

void formatter_renders_markdown_table()
{
    const auto text = perfm::format_markdown(make_samples());
    assert(text.find("| sample_ms | elapsed_time_ms | gpu_percent | cpu_percent |") == 0);
    assert(text.find("| --- | --- | --- | --- |") != std::string::npos);
    assert(perfm::default_markdown_path() == "perfm.md");
}

void formatter_renders_json_samples()
{
    const auto text = perfm::format_json(make_samples());
    assert(text.find("\"samples\"") != std::string::npos);
    assert(text.find("\"gpu_percent\": {\"state\":\"unsupported\",\"message\":\"unsupported\"}") != std::string::npos);
    assert(perfm::default_json_path() == "perfm.json");
}

void formatter_renders_summaries()
{
    const auto summaries = perfm::summarize_samples(make_samples());
    const auto text = perfm::format_csv_summary(summaries);
    assert(text.find("metric,count,min,max,avg,last,unsupported,errors\n") == 0);
    assert(text.find("cpu_percent,1,12.50,12.50,12.50,12.50,0,0") != std::string::npos);
}

void formatter_renders_stdout_graph()
{
    const auto text = perfm::format_stdout_graph(make_samples());
    assert(text.find("CPU") != std::string::npos);
    assert(text.find("Memory") != std::string::npos);
    assert(text.find("█") != std::string::npos);
    assert(text.find("last") != std::string::npos);
}
}

void run_formatter_tests()
{
    formatter_renders_stdout_columns_and_unsupported_values();
    formatter_renders_csv_header_and_rows();
    formatter_renders_markdown_table();
    formatter_renders_json_samples();
    formatter_renders_summaries();
    formatter_renders_stdout_graph();
}
