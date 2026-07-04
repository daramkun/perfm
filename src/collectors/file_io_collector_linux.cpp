#include "collectors/io_collector.h"
#include "process/process_tree.h"

#include <fstream>
#include <string>

namespace perfm
{
namespace
{
class file_io_collector final : public collector
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
        last_read_ = read_process_tree_counter("read_bytes:");
        last_write_ = read_process_tree_counter("write_bytes:");
    }

    std::vector<metric_value> sample() override
    {
        const auto current_read = read_process_tree_counter("read_bytes:");
        const auto current_write = read_process_tree_counter("write_bytes:");
        const auto read_delta = current_read >= last_read_ ? current_read - last_read_ : 0;
        const auto write_delta = current_write >= last_write_ ? current_write - last_write_ : 0;
        last_read_ = current_read;
        last_write_ = current_write;

        return {
            make_metric("file_read_bytes", metric_unit::bytes, static_cast<double>(read_delta)),
            make_metric("file_write_bytes", metric_unit::bytes, static_cast<double>(write_delta)),
        };
    }

    void stop() override
    {
    }

private:
    unsigned long long read_counter(std::uint64_t pid, const std::string& key) const
    {
        std::ifstream input("/proc/" + std::to_string(pid) + "/io");
        std::string label;
        unsigned long long value = 0;
        while (input >> label >> value)
        {
            if (label == key)
            {
                return value;
            }
        }
        return 0;
    }

    unsigned long long read_process_tree_counter(const std::string& key) const
    {
        unsigned long long total = 0;
        for (const auto pid : target_pids())
        {
            total += read_counter(pid, key);
        }
        return total;
    }

    std::vector<std::uint64_t> target_pids() const
    {
        if (scope_ == collector_scope::process_only)
        {
            return {root_pid_};
        }
        return enumerate_process_tree(root_pid_);
    }

    std::uint64_t root_pid_{0};
    collector_scope scope_{collector_scope::process_tree};
    unsigned long long last_read_{0};
    unsigned long long last_write_{0};
};
}

collector_ptr make_file_io_collector()
{
    return std::make_unique<file_io_collector>();
}
}
