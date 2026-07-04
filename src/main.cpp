#include "cli/parser.h"
#include "collectors/cpu_collector.h"
#include "collectors/gpu_collector.h"
#include "collectors/io_collector.h"
#include "collectors/memory_collector.h"
#include "core/sampler.h"
#include "output/formatter.h"
#include "output/output_writer.h"
#include "process/child_process.h"

#include <algorithm>
#include <iostream>
#include <stdexcept>

namespace perfm
{
namespace
{
bool has_metric(const options& opts, metric_kind kind)
{
    return std::find(opts.metrics.begin(), opts.metrics.end(), kind) != opts.metrics.end();
}

collector_list make_selected_collectors(const options& opts)
{
    collector_list collectors;
    if (has_metric(opts, metric_kind::cpu))
    {
        collectors.push_back(make_cpu_collector());
    }
    if (has_metric(opts, metric_kind::mem))
    {
        collectors.push_back(make_memory_collector());
    }
    if (has_metric(opts, metric_kind::file))
    {
        collectors.push_back(make_file_io_collector());
    }
    if (has_metric(opts, metric_kind::network))
    {
        collectors.push_back(make_network_collector());
    }
    if (has_metric(opts, metric_kind::gpu) || has_metric(opts, metric_kind::vmem))
    {
        collectors.push_back(make_gpu_collector(has_metric(opts, metric_kind::gpu), has_metric(opts, metric_kind::vmem)));
    }
    return collectors;
}

std::string format_output(const options& opts, const std::vector<sample>& samples)
{
    switch (opts.mode)
    {
    case output_mode::stdout_table:
        return format_stdout(samples);
    case output_mode::csv:
        return format_csv(samples);
    case output_mode::markdown:
        return format_markdown(samples);
    }
    return {};
}

std::optional<std::string> resolved_output_path(const options& opts)
{
    if (opts.mode == output_mode::stdout_table)
    {
        return std::nullopt;
    }
    if (opts.output_path.has_value())
    {
        return opts.output_path;
    }
    if (opts.mode == output_mode::csv)
    {
        return default_csv_path();
    }
    return default_markdown_path();
}
}
}

int main(int argc, char** argv)
{
    std::vector<std::string> arguments;
    for (int index = 1; index < argc; ++index)
    {
        arguments.emplace_back(argv[index]);
    }

    auto parsed = perfm::parse_options(arguments);
    if (!parsed.ok)
    {
        std::cerr << parsed.error << '\n' << perfm::format_usage() << '\n';
        return 2;
    }

    try
    {
        auto process = perfm::launch_child_process(parsed.value.target_path, parsed.value.target_args);
        auto collectors = perfm::make_selected_collectors(parsed.value);
        perfm::sampler_config config;
        config.frequency = parsed.value.sample_frequency;
        config.include_elapsed_time = perfm::has_metric(parsed.value, perfm::metric_kind::time);
        const auto samples = perfm::sample_process(process, collectors, config);
        const auto output = perfm::format_output(parsed.value, samples);
        const auto output_path = perfm::resolved_output_path(parsed.value);

        std::string error;
        if (!perfm::write_output(output, output_path, error))
        {
            std::cerr << error << '\n';
            return 1;
        }

        return process.exit_code();
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
