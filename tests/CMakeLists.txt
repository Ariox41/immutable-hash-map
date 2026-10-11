add_executable(test_immutable_map test_immutable_map.cpp)
target_link_libraries(test_immutable_map PRIVATE immutable_map)
target_compile_options(test_immutable_map PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>
    $<$<CXX_COMPILER_ID:MSVC>:/W4>
)

add_test(NAME test_immutable_map COMMAND test_immutable_map)
