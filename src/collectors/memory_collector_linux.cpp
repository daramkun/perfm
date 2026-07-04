#include "collectors/memory_collector.h"

#include <fstream>
#include <sstream>
#include <string>
#include <unistd.h>

namespace perfm
{
namespace
{
class memory_collector final : public collector
{
public:
    void start(std::uint64_t pid) override
    {
        pid_ = pid;
        page_size_ = sysconf(_SC_PAGESIZE);
    }

    std::vector<metric_value> sample() override
    {
        return {
            make_metric("memory_total_bytes", metric_unit::bytes, static_cast<double>(read_total_memory())),
            make_metric("memory_resident_bytes", metric_unit::bytes, static_cast<double>(read_status_value("VmRSS:"))),
            make_metric("memory_virtual_bytes", metric_unit::bytes, static_cast<double>(read_status_value("VmSize:"))),
        };
    }

    void stop() override
    {
    }

private:
    unsigned long long read_status_value(const std::string& key) const
    {
        std::ifstream input("/proc/" + std::to_string(pid_) + "/status");
        std::string label;
        unsigned long long value = 0;
        std::string unit;
        while (input >> label >> value >> unit)
        {
            if (label == key)
            {
                return value * 1024;
            }
        }
        return 0;
    }

    unsigned long long read_total_memory() const
    {
        std::ifstream input("/proc/meminfo");
        std::string label;
        unsigned long long value = 0;
        std::string unit;
        while (input >> label >> value >> unit)
        {
            if (label == "MemTotal:")
            {
                return value * 1024;
            }
        }
        return 0;
    }

    std::uint64_t pid_{0};
    long page_size_{4096};
};
}

collector_ptr make_memory_collector()
{
    return std::make_unique<memory_collector>();
}
}
