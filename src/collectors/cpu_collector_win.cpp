#include "collectors/cpu_collector.h"
#include "process/process_tree.h"

#define NOMINMAX
#include <windows.h>
#include <evntrace.h>
#include <evntcons.h>
#include <tlhelp32.h>

#include <atomic>
#include <chrono>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_set>
#include <vector>

namespace perfm
{
namespace
{
constexpr UCHAR context_switch_opcode = 36;
constexpr GUID system_trace_control_guid{0x9e814aad, 0x3204, 0x11d2, {0x9a, 0x82, 0x00, 0x60, 0x08, 0xa8, 0x69, 0x39}};

unsigned long long file_time_to_uint64(const FILETIME& value)
{
    ULARGE_INTEGER integer{};
    integer.LowPart = value.dwLowDateTime;
    integer.HighPart = value.dwHighDateTime;
    return integer.QuadPart;
}

struct context_switch_event
{
    ULONG new_thread_id;
    ULONG old_thread_id;
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

class cpu_collector final : public collector
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
        SYSTEM_INFO info{};
        GetSystemInfo(&info);
        logical_cores_ = std::max<DWORD>(1, info.dwNumberOfProcessors);
        core_seconds_.assign(logical_cores_, 0.0);
        last_cpu_timestamp_.assign(logical_cores_, 0);
        QueryPerformanceFrequency(&performance_frequency_);
        update_target_threads();
        start_etw();
        last_wall_ = std::chrono::steady_clock::now();
        last_process_time_ = read_process_tree_time();
    }

    std::vector<metric_value> sample() override
    {
        update_target_threads();
        const auto wall = std::chrono::steady_clock::now();
        const auto process_time = read_process_tree_time();
        const auto wall_delta = std::chrono::duration<double>(wall - last_wall_).count();
        const auto process_delta = process_time >= last_process_time_ ? process_time - last_process_time_ : 0;
        const auto process_delta_seconds = static_cast<double>(process_delta) / 10000000.0;
        double percent = 0.0;
        if (wall_delta > 0.0)
        {
            percent = (process_delta_seconds / wall_delta) * 100.0;
        }

        last_wall_ = wall;
        last_process_time_ = process_time;

        std::vector<double> per_core_seconds;
        {
            std::lock_guard lock(mutex_);
            per_core_seconds = core_seconds_;
            std::fill(core_seconds_.begin(), core_seconds_.end(), 0.0);
        }

        std::vector<metric_value> values{
            make_metric("cpu_percent", metric_unit::percent, percent),
            make_metric("cpu_total_percent", metric_unit::percent, percent),
        };
        if (etw_running_)
        {
            for (std::size_t index = 0; index < per_core_seconds.size(); ++index)
            {
                double core_percent = 0.0;
                if (wall_delta > 0.0)
                {
                    core_percent = (per_core_seconds[index] / wall_delta) * 100.0;
                }
                values.push_back(make_metric("cpu_" + std::to_string(index + 1) + "_usage", metric_unit::percent, core_percent));
            }
        }
        else if (!etw_error_.empty())
        {
            for (DWORD index = 0; index < logical_cores_; ++index)
            {
                values.push_back(make_error_metric("cpu_" + std::to_string(index + 1) + "_usage", metric_unit::percent, etw_error_));
            }
        }
        return values;
    }

    void stop() override
    {
        stop_etw();
    }

    ~cpu_collector() override
    {
        stop();
    }

private:
    static void WINAPI event_record_callback(EVENT_RECORD* record)
    {
        if (record == nullptr || record->UserContext == nullptr)
        {
            return;
        }
        static_cast<cpu_collector*>(record->UserContext)->handle_event(*record);
    }

    void handle_event(const EVENT_RECORD& record)
    {
        if (record.EventHeader.EventDescriptor.Opcode != context_switch_opcode ||
            record.UserDataLength < sizeof(context_switch_event) || performance_frequency_.QuadPart <= 0)
        {
            return;
        }

        const auto cpu_index = static_cast<std::size_t>(record.BufferContext.ProcessorNumber);
        if (cpu_index >= last_cpu_timestamp_.size())
        {
            return;
        }

        const auto timestamp = record.EventHeader.TimeStamp.QuadPart;
        const auto* data = static_cast<const context_switch_event*>(record.UserData);

        std::lock_guard lock(mutex_);
        const auto previous_timestamp = last_cpu_timestamp_[cpu_index];
        last_cpu_timestamp_[cpu_index] = timestamp;
        if (previous_timestamp == 0 || target_threads_.find(data->old_thread_id) == target_threads_.end())
        {
            return;
        }

        const auto elapsed_ticks = timestamp - previous_timestamp;
        if (elapsed_ticks > 0)
        {
            core_seconds_[cpu_index] += static_cast<double>(elapsed_ticks) / static_cast<double>(performance_frequency_.QuadPart);
        }
    }

    void start_etw()
    {
        if (etw_running_)
        {
            return;
        }

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
                etw_error_ = "ETW context switch tracing unavailable: " + windows_error_message(update_status);
                return;
            }
        }
        else
        {
            etw_error_ = "ETW context switch tracing unavailable: " + windows_error_message(start_status);
            session_handle_ = 0;
            return;
        }

        EVENT_TRACE_LOGFILEW logfile{};
        logfile.LoggerName = session_name_.data();
        logfile.ProcessTraceMode = PROCESS_TRACE_MODE_REAL_TIME | PROCESS_TRACE_MODE_EVENT_RECORD;
        logfile.EventRecordCallback = &cpu_collector::event_record_callback;
        logfile.Context = this;

        trace_handle_ = OpenTraceW(&logfile);
        if (trace_handle_ == INVALID_PROCESSTRACE_HANDLE)
        {
            etw_error_ = "ETW context switch tracing unavailable: " + windows_error_message(GetLastError());
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

    void update_target_threads()
    {
        std::unordered_set<DWORD> threads;
        std::unordered_set<DWORD> pids;
        for (const auto pid : target_pids())
        {
            pids.insert(static_cast<DWORD>(pid));
        }

        const HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
        if (snapshot == INVALID_HANDLE_VALUE)
        {
            return;
        }

        THREADENTRY32 entry{};
        entry.dwSize = sizeof(entry);
        if (Thread32First(snapshot, &entry))
        {
            do
            {
                if (pids.find(entry.th32OwnerProcessID) != pids.end())
                {
                    threads.insert(entry.th32ThreadID);
                }
                entry.dwSize = sizeof(entry);
            } while (Thread32Next(snapshot, &entry));
        }
        CloseHandle(snapshot);

        std::lock_guard lock(mutex_);
        target_threads_ = std::move(threads);
    }

    unsigned long long read_process_time(std::uint64_t pid) const
    {
        const HANDLE process_handle = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, static_cast<DWORD>(pid));
        if (process_handle == nullptr)
        {
            return 0;
        }

        FILETIME creation{};
        FILETIME exit{};
        FILETIME kernel{};
        FILETIME user{};
        if (!GetProcessTimes(process_handle, &creation, &exit, &kernel, &user))
        {
            CloseHandle(process_handle);
            return 0;
        }
        const auto process_time = file_time_to_uint64(kernel) + file_time_to_uint64(user);
        CloseHandle(process_handle);
        return process_time;
    }

    unsigned long long read_process_tree_time() const
    {
        unsigned long long total = 0;
        for (const auto pid : target_pids())
        {
            total += read_process_time(pid);
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
    DWORD logical_cores_{1};
    std::chrono::steady_clock::time_point last_wall_{};
    unsigned long long last_process_time_{0};
    LARGE_INTEGER performance_frequency_{};
    std::mutex mutex_;
    std::unordered_set<DWORD> target_threads_;
    std::vector<double> core_seconds_;
    std::vector<long long> last_cpu_timestamp_;
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

collector_ptr make_cpu_collector()
{
    return std::make_unique<cpu_collector>();
}
}
