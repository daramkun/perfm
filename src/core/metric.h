#pragma once

#include <string>

namespace perfm
{
enum class metric_kind
{
    cpu,
    mem,
    gpu,
    vmem,
    file,
    network,
    time
};

enum class metric_unit
{
    none,
    percent,
    bytes,
    milliseconds
};

enum class metric_state
{
    ok,
    unsupported,
    error
};

struct metric_value
{
    std::string name;
    metric_unit unit{metric_unit::none};
    metric_state state{metric_state::ok};
    double value{0.0};
    std::string message;
};

inline metric_value make_metric(std::string name, metric_unit unit, double value)
{
    return metric_value{std::move(name), unit, metric_state::ok, value, {}};
}

inline metric_value make_unsupported_metric(std::string name, metric_unit unit)
{
    return metric_value{std::move(name), unit, metric_state::unsupported, 0.0, "unsupported"};
}

inline metric_value make_error_metric(std::string name, metric_unit unit, std::string message)
{
    return metric_value{std::move(name), unit, metric_state::error, 0.0, std::move(message)};
}
}
