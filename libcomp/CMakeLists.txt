project(ccomp_lib)
 
include_directories(include)
 
file(GLOB SOURCES "src/*.c")
file(GLOB HEADERS "include/*.h")
 
add_library(ccomp_lib STATIC ${SOURCES} ${HEADERS})
target_include_directories(ccomp_lib INTERFACE ../)
 
