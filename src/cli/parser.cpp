#include "cli/parser.h"

#include "cli/duration_parser.h"

#include <algorithm>

namespace perfm
{
namespace
{
bool starts_with(const std::string& value, const std::string& prefix)
{
    return value.rfind(prefix, 0) == 0;
}

void add_metric(options& opts, metric_kind kind)
{
    if (std::find(opts.metrics.begin(), opts.metrics.end(), kind) == opts.metrics.end())
    {
        opts.metrics.push_back(kind);
    }
}

parse_result error_result(std::string message)
{
    return {false, {}, std::move(message)};
}
}

std::string format_usage()
{
    return "usage: perfm [options] [--summary] [--stdout-graph] [--force-etw] [--split-subproc] -- <application filename> [application arguments]";
}

parse_result parse_options(const std::vector<std::string>& arguments)
{
    options opts;
    opts.sample_frequency = default_sample_frequency();

    std::size_t index = 0;
    bool found_separator = false;
    for (; index < arguments.size(); ++index)
    {
        const auto& arg = arguments[index];
        if (arg == "--")
        {
            found_separator = true;
            ++index;
            break;
        }

        if (arg == "--as-stdout")
        {
            opts.mode = output_mode::stdout_table;
            opts.output_path.reset();
        }
        else if (arg == "--as-csv")
        {
            opts.mode = output_mode::csv;
            opts.output_path.reset();
        }
        else if (starts_with(arg, "--as-csv="))
        {
            opts.mode = output_mode::csv;
            opts.output_path = arg.substr(9);
        }
        else if (arg == "--as-md")
        {
            opts.mode = output_mode::markdown;
            opts.output_path.reset();
        }
        else if (starts_with(arg, "--as-md="))
        {
            opts.mode = output_mode::markdown;
            opts.output_path = arg.substr(8);
        }
        else if (arg == "--as-json")
        {
            opts.mode = output_mode::json;
            opts.output_path.reset();
        }
        else if (starts_with(arg, "--as-json="))
        {
            opts.mode = output_mode::json;
            opts.output_path = arg.substr(10);
        }
        else if (arg == "--cpu")
        {
            add_metric(opts, metric_kind::cpu);
        }
        else if (arg == "--mem")
        {
            add_metric(opts, metric_kind::mem);
        }
        else if (arg == "--gpu")
        {
            add_metric(opts, metric_kind::gpu);
        }
        else if (arg == "--vmem")
        {
            add_metric(opts, metric_kind::vmem);
        }
        else if (arg == "--file")
        {
            add_metric(opts, metric_kind::file);
        }
        else if (arg == "--network")
        {
            add_metric(opts, metric_kind::network);
        }
        else if (arg == "--time")
        {
            add_metric(opts, metric_kind::time);
        }
        else if (arg == "--split-subproc")
        {
            opts.split_subprocesses = true;
        }
        else if (arg == "--summary")
        {
            opts.summary = true;
        }
        else if (arg == "--stdout-graph")
        {
            opts.stdout_graph = true;
        }
        else if (arg == "--force-etw")
        {
            opts.force_etw = true;
        }
        else if (starts_with(arg, "--freq="))
        {
            auto parsed = parse_duration(arg.substr(7));
            if (!parsed.ok)
            {
                return error_result("--freq is invalid: " + parsed.error);
            }
            opts.sample_frequency = parsed.value;
        }
        else
        {
            return error_result("unknown option: " + arg);
        }
    }

    if (!found_separator)
    {
        return error_result("missing -- separator before target application");
    }
    if (index >= arguments.size())
    {
        return error_result("missing target application after --");
    }

    opts.target_path = arguments[index++];
    for (; index < arguments.size(); ++index)
    {
        opts.target_args.push_back(arguments[index]);
    }

    if (opts.stdout_graph && opts.mode != output_mode::stdout_table)
    {
        return error_result("--stdout-graph requires --as-stdout");
    }
    if (opts.stdout_graph && opts.summary)
    {
        return error_result("--stdout-graph cannot be combined with --summary");
    }

    return {true, std::move(opts), {}};
}
}
