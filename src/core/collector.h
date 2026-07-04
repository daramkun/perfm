#pragma once

#include "core/metric.h"

#include <cstdint>
#include <memory>
#include <vector>

namespace perfm
{
class collector
{
public:
    virtual ~collector() = default;

    virtual void start(std::uint64_t pid) = 0;
    virtual std::vector<metric_value> sample() = 0;
    virtual void stop() = 0;
};

using collector_ptr = std::unique_ptr<collector>;
using collector_list = std::vector<collector_ptr>;
}
