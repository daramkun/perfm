#include "collectors/io_collector.h"

#define NOMINMAX
#include <windows.h>

namespace perfm
{
namespace
{
class file_io_collector final : public collector
{
public:
    void start(std::uint64_t pid) override
    {
        process_handle_ = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, static_cast<DWORD>(pid));
        IO_COUNTERS counters{};
        if (process_handle_ != nullptr && GetProcessIoCounters(process_handle_, &counters))
        {
            last_read_ = counters.ReadTransferCount;
            last_write_ = counters.WriteTransferCount;
        }
    }

    std::vector<metric_value> sample() override
    {
        IO_COUNTERS counters{};
        if (process_handle_ == nullptr || !GetProcessIoCounters(process_handle_, &counters))
        {
            return {
                make_error_metric("file_read_bytes", metric_unit::bytes, "unavailable"),
                make_error_metric("file_write_bytes", metric_unit::bytes, "unavailable"),
            };
        }

        const auto read_delta = counters.ReadTransferCount - last_read_;
        const auto write_delta = counters.WriteTransferCount - last_write_;
        last_read_ = counters.ReadTransferCount;
        last_write_ = counters.WriteTransferCount;

        return {
            make_metric("file_read_bytes", metric_unit::bytes, static_cast<double>(read_delta)),
            make_metric("file_write_bytes", metric_unit::bytes, static_cast<double>(write_delta)),
        };
    }

    void stop() override
    {
        if (process_handle_ != nullptr)
        {
            CloseHandle(process_handle_);
            process_handle_ = nullptr;
        }
    }

    ~file_io_collector() override
    {
        stop();
    }

private:
    HANDLE process_handle_{nullptr};
    ULONGLONG last_read_{0};
    ULONGLONG last_write_{0};
};
}

collector_ptr make_file_io_collector()
{
    return std::make_unique<file_io_collector>();
}
}
