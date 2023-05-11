cmake_minimum_required(VERSION 3.19)

project(virt-revive)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/releases)

find_package(Python3 COMPONENTS Interpreter Development REQUIRED)
find_library(NAMES virt REQUIRED)

add_compile_options(-I/usr/include/libvirt -I/usr/include/python3.*)
add_link_options(${Python3_LINK_OPTIONS})
add_executable(virt-revive src/virt-revive.cpp)

target_include_directories(virt-revive PRIVATE src)
target_link_libraries(virt-revive Python3::Python virt)