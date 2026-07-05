#include "collectors/io_collector.h"

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0600
#endif
#ifndef NTDDI_VERSION
#define NTDDI_VERSION 0x06000000
#endif

#define NOMINMAX
#include <winsock2.h>
#include <windows.h>
#include <evntrace.h>
#include <evntcons.h>
#include <netioapi.h>
#include <iphlpapi.h>

#include <atomic>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_set>
#include <vector>

#include "process/process_tree.h"

namespace perfm
{
namespace
{
constexpr GUID system_trace_control_guid{0x9e814aad, 0x3204, 0x11d2, {0x9a, 0x82, 0x00, 0x60, 0x08, 0xa8, 0x69, 0x39}};

struct tcpip_transfer_event
{
    ULONG process_id;
    ULONG size;
};

std::string windows_error_message(DWORD error_code)
{
    LPWSTR buffer = nullptr;
    const DWORD size = FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                                     nullptr,
                                     error_code,
                                     MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                                     reinterpret_cast<LPWSTR>(&buffer),
                                     0,
                                     nullptr);
    if (size == 0 || buffer == nullptr)
    {
        return "error code " + std::to_string(error_code);
    }

    std::wstring wide(buffer, size);
    LocalFree(buffer);
    while (!wide.empty() && (wide.back() == L'\r' || wide.back() == L'\n' || wide.back() == L'.' || wide.back() == L' '))
    {
        wide.pop_back();
    }

    const int needed = WideCharToMultiByte(CP_UTF8, 0, wide.data(), static_cast<int>(wide.size()), nullptr, 0, nullptr, nullptr);
    if (needed <= 0)
    {
        return "error code " + std::to_string(error_code);
    }
    std::string result(static_cast<std::size_t>(needed), '\0');
    WideCharToMultiByte(CP_UTF8, 0, wide.data(), static_cast<int>(wide.size()), result.data(), needed, nullptr, nullptr);
    return result;
}

class network_collector final : public collector
{
public:
    void start(std::uint64_t pid) override
    {
        start(pid, collector_scope::process_tree);
    }

    void start(std::uint64_t pid, collector_scope scope) override
    {
        stop();
        root_pid_ = pid;
        scope_ = scope;
        update_target_pids();
        start_etw();
        const auto counters = read_system_counters();
        last_read_ = counters.read_bytes;
        last_write_ = counters.write_bytes;
    }

    std::vector<metric_value> sample() override
    {
        update_target_pids();
        if (etw_running_)
        {
            unsigned long long read_bytes = 0;
            unsigned long long write_bytes = 0;
            {
                std::lock_guard lock(mutex_);
                read_bytes = read_bytes_;
                write_bytes = write_bytes_;
                read_bytes_ = 0;
                write_bytes_ = 0;
            }
            return {
                make_metric("network_read_bytes", metric_unit::bytes, static_cast<double>(read_bytes)),
                make_metric("network_write_bytes", metric_unit::bytes, static_cast<double>(write_bytes)),
            };
        }

        if (scope_ == collector_scope::process_only)
        {
            if (!etw_error_.empty())
            {
                return {
                    make_error_metric("network_read_bytes", metric_unit::bytes, etw_error_),
                    make_error_metric("network_write_bytes", metric_unit::bytes, etw_error_),
                };
            }
            return {
                make_unsupported_metric("network_read_bytes", metric_unit::bytes),
                make_unsupported_metric("network_write_bytes", metric_unit::bytes),
            };
        }

        const auto counters = read_system_counters();
        if (!counters.has_value)
        {
            return {
                make_error_metric("network_read_bytes", metric_unit::bytes, "unavailable"),
                make_error_metric("network_write_bytes", metric_unit::bytes, "unavailable"),
            };
        }

        const auto read_delta = counters.read_bytes >= last_read_ ? counters.read_bytes - last_read_ : 0;
        const auto write_delta = counters.write_bytes >= last_write_ ? counters.write_bytes - last_write_ : 0;
        last_read_ = counters.read_bytes;
        last_write_ = counters.write_bytes;

        return {
            make_metric("network_read_bytes", metric_unit::bytes, static_cast<double>(read_delta)),
            make_metric("network_write_bytes", metric_unit::bytes, static_cast<double>(write_delta)),
        };
    }

    void stop() override
    {
        stop_etw();
    }

private:
    static void WINAPI event_record_callback(EVENT_RECORD* record)
    {
        if (record == nullptr || record->UserContext == nullptr)
        {
            return;
        }
        static_cast<network_collector*>(record->UserContext)->handle_event(*record);
    }

    void handle_event(const EVENT_RECORD& record)
    {
        const auto opcode = record.EventHeader.EventDescriptor.Opcode;
        if ((opcode != EVENT_TRACE_TYPE_SEND && opcode != EVENT_TRACE_TYPE_RECEIVE) ||
            record.UserDataLength < sizeof(tcpip_transfer_event))
        {
            return;
        }

        const auto* data = static_cast<const tcpip_transfer_event*>(record.UserData);
        std::lock_guard lock(mutex_);
        if (target_pids_.find(data->process_id) == target_pids_.end())
        {
            return;
        }

        if (opcode == EVENT_TRACE_TYPE_RECEIVE)
        {
            read_bytes_ += data->size;
        }
        else
        {
            write_bytes_ += data->size;
        }
    }

    void start_etw()
    {
        session_name_ = KERNEL_LOGGER_NAMEW;
        const auto buffer_size = sizeof(EVENT_TRACE_PROPERTIES) + (session_name_.size() + 1) * sizeof(wchar_t);
        trace_properties_buffer_.assign(buffer_size, 0);
        auto* properties = reinterpret_cast<EVENT_TRACE_PROPERTIES*>(trace_properties_buffer_.data());
        properties->Wnode.BufferSize = static_cast<ULONG>(trace_properties_buffer_.size());
        properties->Wnode.Guid = system_trace_control_guid;
        properties->Wnode.Flags = WNODE_FLAG_TRACED_GUID;
        properties->Wnode.ClientContext = 1;
        properties->LogFileMode = EVENT_TRACE_REAL_TIME_MODE;
        properties->LoggerNameOffset = sizeof(EVENT_TRACE_PROPERTIES);
        properties->EnableFlags = EVENT_TRACE_FLAG_CSWITCH | EVENT_TRACE_FLAG_NETWORK_TCPIP;

        const auto start_status = StartTraceW(&session_handle_, session_name_.data(), properties);
        if (start_status == ERROR_SUCCESS)
        {
            owns_session_ = true;
        }
        else if (start_status == ERROR_ALREADY_EXISTS)
        {
            session_handle_ = 0;
            owns_session_ = false;
            const auto update_status = ControlTraceW(0, session_name_.data(), properties, EVENT_TRACE_CONTROL_UPDATE);
            if (update_status != ERROR_SUCCESS)
            {
                etw_error_ = "ETW TCP/IP tracing unavailable: " + windows_error_message(update_status);
                return;
            }
        }
        else
        {
            etw_error_ = "ETW TCP/IP tracing unavailable: " + windows_error_message(start_status);
            session_handle_ = 0;
            return;
        }

        EVENT_TRACE_LOGFILEW logfile{};
        logfile.LoggerName = session_name_.data();
        logfile.ProcessTraceMode = PROCESS_TRACE_MODE_REAL_TIME | PROCESS_TRACE_MODE_EVENT_RECORD;
        logfile.EventRecordCallback = &network_collector::event_record_callback;
        logfile.Context = this;

        trace_handle_ = OpenTraceW(&logfile);
        if (trace_handle_ == INVALID_PROCESSTRACE_HANDLE)
        {
            etw_error_ = "ETW TCP/IP tracing unavailable: " + windows_error_message(GetLastError());
            if (owns_session_)
            {
                ControlTraceW(session_handle_, session_name_.data(), properties, EVENT_TRACE_CONTROL_STOP);
            }
            session_handle_ = 0;
            trace_handle_ = 0;
            owns_session_ = false;
            return;
        }

        etw_running_ = true;
        etw_error_.clear();
        trace_thread_ = std::thread([this]() {
            TRACEHANDLE handle = trace_handle_;
            ProcessTrace(&handle, 1, nullptr, nullptr);
        });
    }

    void stop_etw()
    {
        if (!etw_running_)
        {
            return;
        }

        etw_running_ = false;
        if (owns_session_ && session_handle_ != 0 && !trace_properties_buffer_.empty())
        {
            auto* properties = reinterpret_cast<EVENT_TRACE_PROPERTIES*>(trace_properties_buffer_.data());
            ControlTraceW(session_handle_, session_name_.data(), properties, EVENT_TRACE_CONTROL_STOP);
        }
        if (trace_handle_ != 0 && trace_handle_ != INVALID_PROCESSTRACE_HANDLE)
        {
            CloseTrace(trace_handle_);
        }
        if (trace_thread_.joinable())
        {
            trace_thread_.join();
        }
        trace_handle_ = 0;
        session_handle_ = 0;
        owns_session_ = false;
    }

    void update_target_pids()
    {
        std::unordered_set<DWORD> pids;
        for (const auto pid : target_pids())
        {
            pids.insert(static_cast<DWORD>(pid));
        }

        std::lock_guard lock(mutex_);
        target_pids_ = std::move(pids);
    }

    std::vector<std::uint64_t> target_pids() const
    {
        if (scope_ == collector_scope::process_only)
        {
            return {root_pid_};
        }
        return enumerate_process_tree(root_pid_);
    }

    struct network_counters
    {
        unsigned long long read_bytes{0};
        unsigned long long write_bytes{0};
        bool has_value{false};
    };

    static network_counters read_system_counters()
    {
        network_counters total;
        MIB_IF_TABLE2* table = nullptr;
        if (GetIfTable2(&table) != NO_ERROR || table == nullptr)
        {
            return total;
        }

        for (ULONG index = 0; index < table->NumEntries; ++index)
        {
            const auto& row = table->Table[index];
            if (row.OperStatus != IfOperStatusUp || row.Type == IF_TYPE_SOFTWARE_LOOPBACK)
            {
                continue;
            }

            total.read_bytes += row.InOctets;
            total.write_bytes += row.OutOctets;
            total.has_value = true;
        }

        FreeMibTable(table);
        return total;
    }

    collector_scope scope_{collector_scope::process_tree};
    std::uint64_t root_pid_{0};
    unsigned long long last_read_{0};
    unsigned long long last_write_{0};
    std::mutex mutex_;
    std::unordered_set<DWORD> target_pids_;
    unsigned long long read_bytes_{0};
    unsigned long long write_bytes_{0};
    std::wstring session_name_;
    std::string etw_error_;
    std::vector<unsigned char> trace_properties_buffer_;
    TRACEHANDLE session_handle_{0};
    TRACEHANDLE trace_handle_{0};
    std::thread trace_thread_;
    std::atomic_bool etw_running_{false};
    bool owns_session_{false};
};
}

collector_ptr make_network_collector()
{
    return std::make_unique<network_collector>();
}
}
