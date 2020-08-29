project(libj_lib)
 
include_directories(include)
 
file(GLOB SOURCES "src/*.c")
file(GLOB HEADERS "include/*.h")
 
add_library(libj_lib STATIC ${SOURCES} ${HEADERS})
target_include_directories(libj_lib INTERFACE ../)
 
