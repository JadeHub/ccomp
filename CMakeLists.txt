cmake_minimum_required(VERSION 3.2)

project(compiler)

set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreadedDebug")

configure_file(CMakeLists.txt.in
        googletest-download/CMakeLists.txt)
execute_process(COMMAND ${CMAKE_COMMAND} -G "${CMAKE_GENERATOR}" .
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/googletest-download )
execute_process(COMMAND ${CMAKE_COMMAND} --build .
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/googletest-download )

add_subdirectory(${CMAKE_BINARY_DIR}/googletest-src
        ${CMAKE_BINARY_DIR}/googletest-build)

enable_testing()

set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreadedDebug")


add_subdirectory(libcomp)
add_subdirectory(compiler)
add_subdirectory(test)
