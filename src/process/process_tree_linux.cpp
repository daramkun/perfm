#include "process/process_tree.h"

#include <dirent.h>

#include <cctype>
#include <fstream>
#include <string>
#include <unordered_map>
#include <unordered_set>

namespace perfm
{
namespace
{
bool is_pid_directory(const char* name)
{
    if (name == nullptr || *name == '\0')
    {
        return false;
    }
    for (const char* cursor = name; *cursor != '\0'; ++cursor)
    {
        if (!std::isdigit(static_cast<unsigned char>(*cursor)))
        {
            return false;
        }
    }
    return true;
}

std::uint64_t read_parent_pid(std::uint64_t pid)
{
    std::ifstream input("/proc/" + std::to_string(pid) + "/stat");
    std::string line;
    std::getline(input, line);
    const auto closing_paren = line.rfind(')');
    if (closing_paren == std::string::npos)
    {
        return 0;
    }

    const auto fields = line.substr(closing_paren + 2);
    const auto first_space = fields.find(' ');
    if (first_space == std::string::npos)
    {
        return 0;
    }

    const auto second_space = fields.find(' ', first_space + 1);
    const auto parent = fields.substr(first_space + 1, second_space - first_space - 1);
    try
    {
        return static_cast<std::uint64_t>(std::stoull(parent));
    }
    catch (...)
    {
        return 0;
    }
}
}

std::vector<std::uint64_t> enumerate_process_tree(std::uint64_t root_pid)
{
    std::vector<std::uint64_t> result;
    if (root_pid == 0)
    {
        return result;
    }

    result.push_back(root_pid);

    DIR* proc = opendir("/proc");
    if (proc == nullptr)
    {
        return result;
    }

    std::unordered_map<std::uint64_t, std::vector<std::uint64_t>> children_by_parent;
    while (const auto* entry = readdir(proc))
    {
        if (!is_pid_directory(entry->d_name))
        {
            continue;
        }
        const auto pid = static_cast<std::uint64_t>(std::stoull(entry->d_name));
        const auto parent_pid = read_parent_pid(pid);
        if (parent_pid != 0)
        {
            children_by_parent[parent_pid].push_back(pid);
        }
    }
    closedir(proc);

    std::unordered_set<std::uint64_t> seen;
    seen.insert(root_pid);
    for (std::size_t index = 0; index < result.size(); ++index)
    {
        const auto found = children_by_parent.find(result[index]);
        if (found == children_by_parent.end())
        {
            continue;
        }
        for (const auto child_pid : found->second)
        {
            if (seen.insert(child_pid).second)
            {
                result.push_back(child_pid);
            }
        }
    }

    return result;
}
}
