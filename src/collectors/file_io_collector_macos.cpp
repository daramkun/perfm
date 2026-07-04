#include "collectors/io_collector.h"

namespace perfm
{
namespace
{
class file_io_collector final : public collector
{
public:
    void start(std::uint64_t) override
    {
    }

    std::vector<metric_value> sample() override
    {
        return {
            make_unsupported_metric("file_read_bytes", metric_unit::bytes),
            make_unsupported_metric("file_write_bytes", metric_unit::bytes),
        };
    }

    void stop() override
    {
    }
};
}

collector_ptr make_file_io_collector()
{
    return std::make_unique<file_io_collector>();
}
}
