add_executable(perfm_sleeper helpers/sleeper.cpp)
perfm_set_warnings(perfm_sleeper)

add_executable(perfm_spawner helpers/spawner.cpp)
target_link_libraries(perfm_spawner PRIVATE perfm_core)
perfm_set_warnings(perfm_spawner)

add_executable(perfm_tests
    duration_parser_tests.cpp
    parser_tests.cpp
    formatter_tests.cpp
    sampler_tests.cpp
)
target_link_libraries(perfm_tests PRIVATE perfm_core)
target_compile_definitions(perfm_tests PRIVATE
    "PERFM_SLEEPER_PATH=\"$<TARGET_FILE:perfm_sleeper>\""
    "PERFM_SPAWNER_PATH=\"$<TARGET_FILE:perfm_spawner>\""
)
perfm_set_warnings(perfm_tests)
add_dependencies(perfm_tests perfm_sleeper perfm_spawner)

add_test(NAME perfm_tests COMMAND perfm_tests)
