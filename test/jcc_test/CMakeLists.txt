project(jcc_test)

file(GLOB SOURCES "*.cpp")
file(GLOB HEADERS "*.h")

add_executable(jcc_test ${SOURCES} ${HEADERS})

set_target_properties(jcc_test PROPERTIES CXX_STANDARD 17)

target_link_libraries(jcc_test libj_lib)
target_link_libraries(jcc_test ccomp_lib)
target_link_libraries(jcc_test gtest_main)
target_link_libraries(jcc_test gmock_main)


