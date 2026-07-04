#pragma once

#include "core/collector.h"
#include "core/sample.h"
#include "process/child_process.h"

#include <chrono>
#include <functional>
#include <vector>

namespace perfm
{
struct sampler_config
{
    std::chrono::milliseconds frequency{std::chrono::seconds(5)};
    bool include_elapsed_time{false};
    bool split_subprocesses{false};
    std::function<collector_list()> collector_factory;
};

std::vector<sample> sample_process(child_process& process, collector_list& collectors, const sampler_config& config);
}
