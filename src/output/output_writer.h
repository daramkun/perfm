#pragma once

#include <fstream>
#include <iostream>
#include <optional>
#include <string>

namespace perfm
{
inline bool write_output(const std::string& content, const std::optional<std::string>& output_path, std::string& error)
{
    if (!output_path.has_value())
    {
        std::cout << content;
        return true;
    }

    std::ofstream output(*output_path, std::ios::binary);
    if (!output)
    {
        error = "failed to open output file: " + *output_path;
        return false;
    }

    output << content;
    return true;
}
}
