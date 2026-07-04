#include "collectors/memory_collector.h"
#include "process/process_tree.h"

#include <fstream>
#include <sstream>
#include <string>

namespace perfm
{
namespace
{
class memory_collector final : public collector
{
public:
    void start(std::uint64_t pid) override
    {
        start(pid, collector_scope::process_tree);
    }

    void start(std::uint64_t pid, collector_scope scope) override
    {
        root_pid_ = pid;
        scope_ = scope;
    }

    std::vector<metric_value> sample() override
    {
        unsigned long long resident_bytes = 0;
        unsigned long long virtual_bytes = 0;
        for (const auto pid : target_pids())
        {
            resident_bytes += read_status_value(pid, "VmRSS:");
            virtual_bytes += read_status_value(pid, "VmSize:");
        }

        return {
            make_metric("memory_total_bytes", metric_unit::bytes, static_cast<double>(read_total_memory())),
            make_metric("memory_resident_bytes", metric_unit::bytes, static_cast<double>(resident_bytes)),
            make_metric("memory_virtual_bytes", metric_unit::bytes, static_cast<double>(virtual_bytes)),
        };
    }

    void stop() override
    {
    }

private:
    std::vector<std::uint64_t> target_pids() const
    {
        if (scope_ == collector_scope::process_only)
        {
            return {root_pid_};
        }
        return enumerate_process_tree(root_pid_);
    }

    unsigned long long read_status_value(std::uint64_t pid, const std::string& key) const
    {
        std::ifstream input("/proc/" + std::to_string(pid) + "/status");
        std::string line;
        while (std::getline(input, line))
        {
            if (line.rfind(key, 0) != 0)
            {
                continue;
            }

            std::istringstream fields(line.substr(key.size()));
            unsigned long long value = 0;
            std::string unit;
            if (fields >> value >> unit)
            {
                return value * 1024;
            }
            return 0;
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

    std::uint64_t root_pid_{0};
    collector_scope scope_{collector_scope::process_tree};
};
}

collector_ptr make_memory_collector()
{
    return std::make_unique<memory_collector>();
}
}
