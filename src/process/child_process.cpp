#include "process/child_process.h"

#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#else
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

#include <utility>

namespace perfm
{
child_process::child_process(child_process&& other) noexcept
{
    *this = std::move(other);
}

child_process& child_process::operator=(child_process&& other) noexcept
{
    if (this == &other)
    {
        return *this;
    }

    terminate();

#ifdef _WIN32
    process_handle_ = other.process_handle_;
    thread_handle_ = other.thread_handle_;
    process_id_ = other.process_id_;
    other.process_handle_ = nullptr;
    other.thread_handle_ = nullptr;
    other.process_id_ = 0;
#else
    process_id_ = other.process_id_;
    other.process_id_ = -1;
#endif
    exit_code_ = other.exit_code_;
    other.exit_code_.reset();
    return *this;
}

child_process::~child_process()
{
#ifdef _WIN32
    if (thread_handle_ != nullptr)
    {
        CloseHandle(static_cast<HANDLE>(thread_handle_));
    }
    if (process_handle_ != nullptr)
    {
        CloseHandle(static_cast<HANDLE>(process_handle_));
    }
#endif
}

std::uint64_t child_process::pid() const
{
#ifdef _WIN32
    return process_id_;
#else
    return process_id_ < 0 ? 0 : static_cast<std::uint64_t>(process_id_);
#endif
}

void child_process::terminate()
{
#ifdef _WIN32
    if (process_handle_ != nullptr && is_running())
    {
        TerminateProcess(static_cast<HANDLE>(process_handle_), 1);
        WaitForSingleObject(static_cast<HANDLE>(process_handle_), INFINITE);
    }
#else
    if (process_id_ > 0 && is_running())
    {
        kill(process_id_, SIGTERM);
        wait();
    }
#endif
}
}
