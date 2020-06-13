project(jcc_test)

file(GLOB SOURCES "*.cpp")

add_executable(jcc_test ${SOURCES})


target_link_libraries(jcc_test ccomp_lib)
target_link_libraries(jcc_test gtest_main)
target_link_libraries(jcc_test gmock_main)


