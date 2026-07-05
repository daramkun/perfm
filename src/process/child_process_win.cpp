#include "process/child_process.h"

#define NOMINMAX
#include <windows.h>

#include <stdexcept>
#include <string>

namespace perfm
{
namespace
{
std::wstring to_wide(const std::string& value)
{
    if (value.empty())
    {
        return {};
    }

    const int needed = MultiByteToWideChar(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), nullptr, 0);
    if (needed <= 0)
    {
        throw std::runtime_error("failed to convert command argument to UTF-16");
    }

    std::wstring result(static_cast<std::size_t>(needed), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), result.data(), needed);
    return result;
}

std::wstring quote_argument(const std::wstring& value)
{
    if (value.empty())
    {
        return L"\"\"";
    }

    bool needs_quotes = false;
    for (wchar_t ch : value)
    {
        if (ch == L' ' || ch == L'\t' || ch == L'"')
        {
            needs_quotes = true;
            break;
        }
    }

    if (!needs_quotes)
    {
        return value;
    }

    std::wstring result = L"\"";
    std::size_t backslashes = 0;
    for (wchar_t ch : value)
    {
        if (ch == L'\\')
        {
            ++backslashes;
        }
        else if (ch == L'"')
        {
            result.append(backslashes * 2 + 1, L'\\');
            result.push_back(ch);
            backslashes = 0;
        }
        else
        {
            result.append(backslashes, L'\\');
            result.push_back(ch);
            backslashes = 0;
        }
    }
    result.append(backslashes * 2, L'\\');
    result.push_back(L'"');
    return result;
}

std::wstring build_command_line(const std::string& target_path, const std::vector<std::string>& target_args)
{
    std::wstring command = quote_argument(to_wide(target_path));
    for (const auto& arg : target_args)
    {
        command.push_back(L' ');
        command += quote_argument(to_wide(arg));
    }
    return command;
}

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
}

child_process launch_child_process(const std::string& target_path, const std::vector<std::string>& target_args)
{
    STARTUPINFOW startup_info{};
    startup_info.cb = sizeof(startup_info);
    PROCESS_INFORMATION process_info{};

    std::wstring command_line = build_command_line(target_path, target_args);
    if (!CreateProcessW(nullptr,
                        command_line.data(),
                        nullptr,
                        nullptr,
                        FALSE,
                        0,
                        nullptr,
                        nullptr,
                        &startup_info,
                        &process_info))
    {
        throw std::runtime_error("failed to launch target process: " + windows_error_message(GetLastError()));
    }

    child_process process;
    process.process_handle_ = process_info.hProcess;
    process.thread_handle_ = process_info.hThread;
    process.process_id_ = process_info.dwProcessId;
    return process;
}

bool child_process::is_running()
{
    if (process_handle_ == nullptr)
    {
        return false;
    }

    const DWORD result = WaitForSingleObject(static_cast<HANDLE>(process_handle_), 0);
    if (result == WAIT_TIMEOUT)
    {
        return true;
    }

    if (result == WAIT_OBJECT_0 && !exit_code_.has_value())
    {
        DWORD code = 0;
        GetExitCodeProcess(static_cast<HANDLE>(process_handle_), &code);
        exit_code_ = static_cast<int>(code);
    }
    return false;
}

int child_process::wait()
{
    if (process_handle_ == nullptr)
    {
        return exit_code_.value_or(-1);
    }

    WaitForSingleObject(static_cast<HANDLE>(process_handle_), INFINITE);
    DWORD code = 0;
    GetExitCodeProcess(static_cast<HANDLE>(process_handle_), &code);
    exit_code_ = static_cast<int>(code);
    return *exit_code_;
}

int child_process::exit_code()
{
    if (!exit_code_.has_value())
    {
        is_running();
    }
    return exit_code_.value_or(-1);
}
}
