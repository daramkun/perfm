#include "collectors/gpu_collector.h"

namespace perfm
{
namespace
{
class unsupported_gpu_collector final : public collector
{
public:
    unsupported_gpu_collector(bool include_gpu_percent, bool include_vram)
        : include_gpu_percent_(include_gpu_percent),
          include_vram_(include_vram)
    {
    }

    void start(std::uint64_t) override
    {
    }

    std::vector<metric_value> sample() override
    {
        std::vector<metric_value> values;
        if (include_gpu_percent_)
        {
            values.push_back(make_unsupported_metric("gpu_percent", metric_unit::percent));
        }
        if (include_vram_)
        {
            values.push_back(make_unsupported_metric("gpu_vram_bytes", metric_unit::bytes));
        }
        return values;
    }

    void stop() override
    {
    }

private:
    bool include_gpu_percent_{false};
    bool include_vram_{false};
};
}

collector_ptr make_gpu_collector(bool include_gpu_percent, bool include_vram)
{
    return std::make_unique<unsupported_gpu_collector>(include_gpu_percent, include_vram);
}
}
