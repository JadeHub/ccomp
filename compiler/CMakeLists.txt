project(jcc)

include_directories(include)

file(GLOB SOURCES "*.c")

add_executable(jcc  ${SOURCES})
target_link_libraries(jcc ccomp_lib)

