#include "process/child_process.h"

#include <chrono>
#include <cstdlib>
#include <string>
#include <thread>
#include <vector>

int main(int argc, char** argv)
{
    if (argc < 2)
    {
        return 2;
    }

    std::vector<std::string> child_args;
    for (int index = 2; index < argc; ++index)
    {
        child_args.emplace_back(argv[index]);
    }

    auto child = perfm::launch_child_process(argv[1], child_args);
    while (child.is_running())
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(25));
    }
    return child.exit_code();
}
