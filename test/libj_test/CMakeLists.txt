project(libj_test)

file(GLOB SOURCES "*.cpp")

add_executable(libj_test ${SOURCES})

target_link_libraries(libj_test libj_lib)
target_link_libraries(libj_test gtest_main)
target_link_libraries(libj_test gmock_main)


