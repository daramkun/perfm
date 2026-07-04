#include "collectors/memory_collector.h"

namespace perfm
{
namespace
{
class memory_collector final : public collector
{
public:
    void start(std::uint64_t) override
    {
    }

    std::vector<metric_value> sample() override
    {
        return {
            make_unsupported_metric("memory_total_bytes", metric_unit::bytes),
            make_unsupported_metric("memory_resident_bytes", metric_unit::bytes),
            make_unsupported_metric("memory_virtual_bytes", metric_unit::bytes),
        };
    }

    void stop() override
    {
    }
};
}

collector_ptr make_memory_collector()
{
    return std::make_unique<memory_collector>();
}
}
