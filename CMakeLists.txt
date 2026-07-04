cmake_minimum_required(VERSION 3.20)

project(perfm VERSION 0.1.0 LANGUAGES CXX)

include(CTest)
include(cmake/compiler_warnings.cmake)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(perfm_common_sources
    src/cli/duration_parser.cpp
    src/cli/parser.cpp
    src/core/sampler.cpp
    src/output/csv_formatter.cpp
    src/output/markdown_formatter.cpp
    src/output/stdout_formatter.cpp
    src/process/child_process.cpp
    src/collectors/gpu_collector.cpp
)

if(WIN32)
    list(APPEND perfm_platform_sources
        src/process/child_process_win.cpp
        src/process/process_tree_win.cpp
        src/collectors/cpu_collector_win.cpp
        src/collectors/memory_collector_win.cpp
        src/collectors/file_io_collector_win.cpp
        src/collectors/network_collector_win.cpp
    )
elseif(APPLE)
    list(APPEND perfm_platform_sources
        src/process/child_process_posix.cpp
        src/process/process_tree_macos.cpp
        src/collectors/cpu_collector_macos.cpp
        src/collectors/memory_collector_macos.cpp
        src/collectors/file_io_collector_macos.cpp
        src/collectors/network_collector_macos.cpp
    )
else()
    list(APPEND perfm_platform_sources
        src/process/child_process_posix.cpp
        src/process/process_tree_linux.cpp
        src/collectors/cpu_collector_linux.cpp
        src/collectors/memory_collector_linux.cpp
        src/collectors/file_io_collector_linux.cpp
        src/collectors/network_collector_linux.cpp
    )
endif()

add_library(perfm_core STATIC
    ${perfm_common_sources}
    ${perfm_platform_sources}
)
target_include_directories(perfm_core PUBLIC src)
perfm_set_warnings(perfm_core)

if(WIN32)
    target_link_libraries(perfm_core PUBLIC psapi)
endif()

add_executable(perfm src/main.cpp)
target_link_libraries(perfm PRIVATE perfm_core)
perfm_set_warnings(perfm)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
