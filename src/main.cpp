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

#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#include <shellapi.h>
#endif

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
    if (opts.summary)
    {
        const auto summaries = summarize_samples(samples);
        switch (opts.mode)
        {
        case output_mode::stdout_table:
            return format_stdout_summary(summaries);
        case output_mode::csv:
            return format_csv_summary(summaries);
        case output_mode::markdown:
            return format_markdown_summary(summaries);
        case output_mode::json:
            return format_json_summary(summaries);
        }
    }

    switch (opts.mode)
    {
    case output_mode::stdout_table:
        if (opts.stdout_graph)
        {
            return format_stdout_graph(samples);
        }
        return format_stdout(samples);
    case output_mode::csv:
        return format_csv(samples);
    case output_mode::markdown:
        return format_markdown(samples);
    case output_mode::json:
        return format_json(samples);
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
    if (opts.mode == output_mode::json)
    {
        return default_json_path();
    }
    return default_markdown_path();
}

#ifdef _WIN32
std::wstring to_wide(const std::string& value)
{
    if (value.empty())
    {
        return {};
    }

    const int needed = MultiByteToWideChar(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), nullptr, 0);
    if (needed <= 0)
    {
        throw std::runtime_error("failed to convert command argument to UTF-16");
    }

    std::wstring result(static_cast<std::size_t>(needed), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), result.data(), needed);
    return result;
}

std::wstring quote_argument(const std::wstring& value)
{
    if (value.empty())
    {
        return L"\"\"";
    }

    bool needs_quotes = false;
    for (const auto ch : value)
    {
        if (ch == L' ' || ch == L'\t' || ch == L'"')
        {
            needs_quotes = true;
            break;
        }
    }
    if (!needs_quotes)
    {
        return value;
    }

    std::wstring result = L"\"";
    std::size_t backslashes = 0;
    for (const auto ch : value)
    {
        if (ch == L'\\')
        {
            ++backslashes;
        }
        else if (ch == L'"')
        {
            result.append(backslashes * 2 + 1, L'\\');
            result.push_back(ch);
            backslashes = 0;
        }
        else
        {
            result.append(backslashes, L'\\');
            result.push_back(ch);
            backslashes = 0;
        }
    }
    result.append(backslashes * 2, L'\\');
    result.push_back(L'"');
    return result;
}

bool is_running_elevated()
{
    BOOL is_member = FALSE;
    SID_IDENTIFIER_AUTHORITY authority = SECURITY_NT_AUTHORITY;
    PSID admin_group = nullptr;
    if (!AllocateAndInitializeSid(&authority, 2, SECURITY_BUILTIN_DOMAIN_RID, DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0, &admin_group))
    {
        return false;
    }
    const BOOL ok = CheckTokenMembership(nullptr, admin_group, &is_member);
    FreeSid(admin_group);
    return ok != FALSE && is_member != FALSE;
}

std::wstring current_executable_path()
{
    std::wstring path(MAX_PATH, L'\0');
    DWORD size = GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
    while (size == path.size())
    {
        path.resize(path.size() * 2, L'\0');
        size = GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
    }
    if (size == 0)
    {
        throw std::runtime_error("failed to resolve current executable path");
    }
    path.resize(size);
    return path;
}

std::wstring build_relaunch_parameters(const std::vector<std::string>& arguments)
{
    std::wstring parameters;
    for (std::size_t index = 0; index < arguments.size(); ++index)
    {
        if (index > 0)
        {
            parameters.push_back(L' ');
        }
        parameters += quote_argument(to_wide(arguments[index]));
    }
    return parameters;
}

int relaunch_elevated_and_wait(const std::vector<std::string>& arguments)
{
    SHELLEXECUTEINFOW info{};
    info.cbSize = sizeof(info);
    info.fMask = SEE_MASK_NOCLOSEPROCESS;
    const auto executable = current_executable_path();
    const auto parameters = build_relaunch_parameters(arguments);
    info.lpVerb = L"runas";
    info.lpFile = executable.c_str();
    info.lpParameters = parameters.c_str();
    info.nShow = SW_SHOWNORMAL;

    if (!ShellExecuteExW(&info))
    {
        const auto error = GetLastError();
        if (error == ERROR_CANCELLED)
        {
            throw std::runtime_error("--force-etw elevation was cancelled");
        }
        throw std::runtime_error("--force-etw failed to launch elevated process");
    }

    WaitForSingleObject(info.hProcess, INFINITE);
    DWORD exit_code = 1;
    GetExitCodeProcess(info.hProcess, &exit_code);
    CloseHandle(info.hProcess);
    return static_cast<int>(exit_code);
}
#endif
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
#ifdef _WIN32
        if (parsed.value.force_etw && !perfm::is_running_elevated())
        {
            return perfm::relaunch_elevated_and_wait(arguments);
        }
#endif

        auto process = perfm::launch_child_process(parsed.value.target_path, parsed.value.target_args);
        auto collectors = perfm::make_selected_collectors(parsed.value);
        perfm::sampler_config config;
        config.frequency = parsed.value.sample_frequency;
        config.include_elapsed_time = perfm::has_metric(parsed.value, perfm::metric_kind::time);
        config.split_subprocesses = parsed.value.split_subprocesses;
        config.collector_factory = [&parsed]() {
            return perfm::make_selected_collectors(parsed.value);
        };
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
