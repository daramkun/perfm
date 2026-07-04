#include "process/child_process.h"

#include <cerrno>
#include <cstring>
#include <stdexcept>
#include <sys/wait.h>
#include <unistd.h>

namespace perfm
{
child_process launch_child_process(const std::string& target_path, const std::vector<std::string>& target_args)
{
    const pid_t child_pid = fork();
    if (child_pid < 0)
    {
        throw std::runtime_error("failed to fork target process");
    }

    if (child_pid == 0)
    {
        std::vector<char*> argv;
        argv.push_back(const_cast<char*>(target_path.c_str()));
        for (const auto& arg : target_args)
        {
            argv.push_back(const_cast<char*>(arg.c_str()));
        }
        argv.push_back(nullptr);
        execvp(target_path.c_str(), argv.data());
        _exit(127);
    }

    child_process process;
    process.process_id_ = child_pid;
    return process;
}

bool child_process::is_running()
{
    if (process_id_ <= 0)
    {
        return false;
    }

    int status = 0;
    const pid_t result = waitpid(process_id_, &status, WNOHANG);
    if (result == 0)
    {
        return true;
    }
    if (result == process_id_)
    {
        if (WIFEXITED(status))
        {
            exit_code_ = WEXITSTATUS(status);
        }
        else if (WIFSIGNALED(status))
        {
            exit_code_ = 128 + WTERMSIG(status);
        }
        else
        {
            exit_code_ = -1;
        }
        process_id_ = -1;
        return false;
    }

    if (errno == ECHILD)
    {
        process_id_ = -1;
        return false;
    }
    return false;
}

int child_process::wait()
{
    if (process_id_ <= 0)
    {
        return exit_code_.value_or(-1);
    }

    int status = 0;
    while (waitpid(process_id_, &status, 0) < 0)
    {
        if (errno != EINTR)
        {
            exit_code_ = -1;
            return *exit_code_;
        }
    }

    if (WIFEXITED(status))
    {
        exit_code_ = WEXITSTATUS(status);
    }
    else if (WIFSIGNALED(status))
    {
        exit_code_ = 128 + WTERMSIG(status);
    }
    else
    {
        exit_code_ = -1;
    }
    process_id_ = -1;
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
