#include "collectors/cpu_collector.h"

namespace perfm
{
namespace
{
class cpu_collector final : public collector
{
public:
    void start(std::uint64_t) override
    {
    }

    std::vector<metric_value> sample() override
    {
        return {
            make_unsupported_metric("cpu_percent", metric_unit::percent),
            make_unsupported_metric("cpu_percent_per_logical_core", metric_unit::percent),
            make_unsupported_metric("cpu_percent_per_physical_core", metric_unit::percent),
        };
    }

    void stop() override
    {
    }
};
}

collector_ptr make_cpu_collector()
{
    return std::make_unique<cpu_collector>();
}
}
