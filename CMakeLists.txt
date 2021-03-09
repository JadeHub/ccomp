cmake_minimum_required(VERSION 3.2)

project(compiler)

IF (WIN32)

add_definitions("/D_CRT_SECURE_NO_WARNINGS /W4 /wd6387 /wd4996 /wd6011")

set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreadedDebug")

FOREACH (MODE "_DEBUG" "_MINSIZEREL" "_RELEASE" "_RELWITHDEBINFO")
STRING(REPLACE "/MT" "/MD" TMP "${CMAKE_C_FLAGS${MODE}}")
SET(CMAKE_C_FLAGS${MODE} "${TMP}" CACHE STRING "" FORCE)
MESSAGE(STATUS "C_${MODE}=${CMAKE_C_FLAGS${MODE}}")
STRING(REPLACE "/MT" "/MM" TMP "${CMAKE_CXX_FLAGS${MODE}}")
SET(CMAKE_CXX_FLAGS${MODE} "${TMP}" CACHE STRING "" FORCE)
MESSAGE(STATUS "CXX_${MODE}=${CMAKE_CXX_FLAGS${MODE}}")
ENDFOREACH (MODE)


ELSE (WIN32)

add_definitions("-g")

ENDIF (WIN32)

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

add_subdirectory(libj)
add_subdirectory(libcomp)
add_subdirectory(compiler)
add_subdirectory(test)
