cmake_minimum_required(VERSION 3.10)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
project(gpaste-fzf-out)

find_package(LibXml2 REQUIRED)

include_directories(${LIBXML2_INCLUDE_DIR})
add_executable(gpaste-fzf-out main.c)
target_link_libraries(gpaste-fzf-out ${LIBXML2_LIBRARIES})
