#include "cli/parser.h"

#include <cassert>
#include <chrono>
#include <string>
#include <vector>

namespace
{
bool contains_metric(const perfm::options& opts, perfm::metric_kind kind)
{
    for (const auto value : opts.metrics)
    {
        if (value == kind)
        {
            return true;
        }
    }
    return false;
}

void parser_preserves_target_arguments_after_separator()
{
    const std::vector<std::string> args{
        "--as-csv=out.csv",
        "--time",
        "--freq=1s",
        "--",
        "tool",
        "--target-option",
        "value",
    };

    auto parsed = perfm::parse_options(args);
    assert(parsed.ok);
    assert(parsed.value.mode == perfm::output_mode::csv);
    assert(parsed.value.output_path == "out.csv");
    assert(parsed.value.sample_frequency == std::chrono::seconds(1));
    assert(contains_metric(parsed.value, perfm::metric_kind::time));
    assert(parsed.value.target_path == "tool");
    assert(parsed.value.target_args.size() == 2);
    assert(parsed.value.target_args[0] == "--target-option");
}

void parser_uses_default_output_paths_later()
{
    auto csv = perfm::parse_options({"--as-csv", "--cpu", "--", "tool"});
    assert(csv.ok);
    assert(csv.value.mode == perfm::output_mode::csv);
    assert(!csv.value.output_path.has_value());

    auto markdown = perfm::parse_options({"--as-md", "--mem", "--", "tool"});
    assert(markdown.ok);
    assert(markdown.value.mode == perfm::output_mode::markdown);
    assert(!markdown.value.output_path.has_value());
}

void parser_rejects_invalid_frequency()
{
    auto parsed = perfm::parse_options({"--freq=0s", "--", "tool"});
    assert(!parsed.ok);
    assert(parsed.error.find("--freq") != std::string::npos);
}

void parser_accepts_split_subprocess_option()
{
    auto parsed = perfm::parse_options({"--split-subproc", "--cpu", "--", "tool"});
    assert(parsed.ok);
    assert(parsed.value.split_subprocesses);
    assert(contains_metric(parsed.value, perfm::metric_kind::cpu));
}
}

void run_parser_tests()
{
    parser_preserves_target_arguments_after_separator();
    parser_uses_default_output_paths_later();
    parser_rejects_invalid_frequency();
    parser_accepts_split_subprocess_option();
}
