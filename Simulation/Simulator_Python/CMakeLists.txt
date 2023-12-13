cmake_minimum_required(VERSION 3.15)
project(simC)

set(CMAKE_CXX_STANDARD 17)

# Set the build output location to a common directory
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/output)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/output)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/output)

set(Boost_USE_STATIC_LIBS OFF) 
set(Boost_USE_MULTITHREADED ON)  
set(Boost_USE_STATIC_RUNTIME OFF) 

# Find the Boost program options library
find_package(Boost 1.74.0 COMPONENTS program_options REQUIRED)
if(Boost_FOUND)
  include_directories(${Boost_INCLUDE_DIRS})
endif()
message(STATUS "Boost version: ${Boost_VERSION}")

add_subdirectory(OpenXLSX)

add_executable(simC simulator_interface_ai.cpp)
target_link_libraries(simC OpenXLSX::OpenXLSX Boost::program_options)  # Link Boost program_options library
