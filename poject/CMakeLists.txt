cmake_minimum_required(VERSION 3.16)
project(task_cli CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(task_core STATIC task.cpp)
target_include_directories(task_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
if(MSVC)
    target_compile_options(task_core PRIVATE /W4)
else()
    target_compile_options(task_core PRIVATE -Wall -Wextra -Wpedantic)
    # 静态链接 libstdc++/libgcc，生成的 exe 换一台机器也能直接运行
    if(WIN32)
        add_link_options(-static)
    endif()
endif()

add_executable(task_cli main.cpp)
target_link_libraries(task_cli PRIVATE task_core)

add_executable(task_server server.cpp)
target_link_libraries(task_server PRIVATE task_core)
if(WIN32)
    target_link_libraries(task_server PRIVATE ws2_32 shell32)
endif()

enable_testing()
add_executable(task_tests test.cpp)
target_link_libraries(task_tests PRIVATE task_core)
add_test(NAME task_tests COMMAND task_tests)
