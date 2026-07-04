#include "cli/duration_parser.h"

#include <cassert>
#include <chrono>
#include <iostream>

void run_parser_tests();
void run_formatter_tests();
void run_sampler_tests();

namespace
{
void parse_duration_accepts_documented_units()
{
    auto seconds = perfm::parse_duration("5s");
    assert(seconds.ok);
    assert(seconds.value == std::chrono::seconds(5));

    auto minutes = perfm::parse_duration("1m");
    assert(minutes.ok);
    assert(minutes.value == std::chrono::minutes(1));

    auto milliseconds = perfm::parse_duration("250ms");
    assert(milliseconds.ok);
    assert(milliseconds.value == std::chrono::milliseconds(250));
}

void parse_duration_rejects_zero_and_unknown_units()
{
    auto zero = perfm::parse_duration("0s");
    assert(!zero.ok);

    auto unknown = perfm::parse_duration("1q");
    assert(!unknown.ok);
}
}

int main()
{
    parse_duration_accepts_documented_units();
    parse_duration_rejects_zero_and_unknown_units();
    run_parser_tests();
    run_formatter_tests();
    run_sampler_tests();
    std::cout << "perfm_tests passed\n";
    return 0;
}
