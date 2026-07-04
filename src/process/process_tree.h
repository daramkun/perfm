#pragma once

#include <cstdint>
#include <vector>

namespace perfm
{
std::vector<std::uint64_t> enumerate_process_tree(std::uint64_t root_pid);
}
