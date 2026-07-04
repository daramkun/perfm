#pragma once

#include "core/metric.h"

#include <cstdint>
#include <memory>
#include <vector>

namespace perfm
{
enum class collector_scope
{
    process_only,
    process_tree
};

class collector
{
public:
    virtual ~collector() = default;

    virtual void start(std::uint64_t pid) = 0;
    virtual void start(std::uint64_t pid, collector_scope scope)
    {
        (void)scope;
        start(pid);
    }
    virtual std::vector<metric_value> sample() = 0;
    virtual void stop() = 0;
};

using collector_ptr = std::unique_ptr<collector>;
using collector_list = std::vector<collector_ptr>;
}
