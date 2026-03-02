cmake_minimum_required(VERSION 3.5)
project("PubSubRedundancy")
set(CMAKE_BUILD_TYPE Debug)
add_executable(pubsub_redundancy pubsub_redundancy.c)

# Linux/Unix configuration using pkg-config
find_package(PkgConfig)
pkg_check_modules(open62541 REQUIRED open62541)
target_link_libraries(pubsub_redundancy open62541)

# Alternative CMake-based library definition.
# This might not be included in some package distributions.
#
#   find_package(open62541 REQUIRED)
#   target_link_libraries(main open62541::open62541)
target_include_directories(pubsub_redundancy PRIVATE ${PROJECT_SOURCE_DIR}/../include)

# Set the output directory for the binary
set_target_properties(pubsub_redundancy PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}/../build/bin/redundancy")
