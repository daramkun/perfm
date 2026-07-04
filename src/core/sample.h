#pragma once

#include "core/metric.h"

#include <chrono>
#include <vector>

namespace perfm
{
struct sample
{
    std::chrono::milliseconds elapsed{0};
    std::vector<metric_value> values;
};
}
