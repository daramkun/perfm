#include <chrono>
#include <cstdlib>
#include <string>
#include <thread>

int main(int argc, char** argv)
{
    int milliseconds = 250;
    if (argc > 1)
    {
        milliseconds = std::stoi(argv[1]);
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds));
    return 0;
}
