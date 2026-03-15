cmake_minimum_required(VERSION 3.5)
project("PubSubRedundancy")
set(CMAKE_BUILD_TYPE Debug)
add_executable(pubsub_redundancy pubsub_redundancy.c pubsub_heartbeat.c pubsub_sync.c pubsub_publisher.c)

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

# ---------------------------------------------
# PubSub Subscriber executable
add_executable(pubsub_subscriber
    pubsub_subscriber.c
)
target_link_libraries(pubsub_subscriber PRIVATE open62541)
target_include_directories(pubsub_subscriber PRIVATE ${PROJECT_SOURCE_DIR}/../include)
set_target_properties(pubsub_subscriber PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}/../build/bin/redundancy"
)

# ---------------------------------------------
# PubSub Publisher executable
# add_executable(pubsub_publisher
#     pubsub_publisher.c
# )
# target_link_libraries(pubsub_publisher PRIVATE open62541)
# target_include_directories(pubsub_publisher PRIVATE ${PROJECT_SOURCE_DIR}/../include)
# set_target_properties(pubsub_publisher PROPERTIES
#     RUNTIME_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}/../build/bin/redundancy"
# )
