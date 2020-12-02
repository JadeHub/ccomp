project(jcc_test)

file(GLOB SOURCES "*.cpp")
file(GLOB HEADERS "*.h")

add_executable(jcc_test ${SOURCES} ${HEADERS})

target_link_libraries(jcc_test libj_lib)
target_link_libraries(jcc_test ccomp_lib)
target_link_libraries(jcc_test gtest_main)
target_link_libraries(jcc_test gmock_main)


