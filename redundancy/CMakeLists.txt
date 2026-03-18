cmake_minimum_required(VERSION 3.5)
project("PubSubRedundancy")
set(CMAKE_BUILD_TYPE Debug)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
file(GLOB REDUNDANCY_SRC "${PROJECT_SOURCE_DIR}/src/*.c")
# add_executable(pubsub_redundancy ${REDUNDANCY_SRC})
# add_executable(pubsub_redundancy
#     "src/pubsub_redundancy.c"
#     "src/pubsub_heartbeat.c"
#     "src/pubsub_sync.c"
#     "src/pubsub_publisher.c"
# )

find_package(PkgConfig)
pkg_check_modules(open62541 REQUIRED open62541)
# target_link_libraries(pubsub_redundancy open62541)

# target_include_directories(pubsub_redundancy PRIVATE ${PROJECT_SOURCE_DIR}/include)

# set_target_properties(pubsub_redundancy PROPERTIES
#     RUNTIME_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}/../build/bin/redundancy"
# )

# ---------------------------------------------
# PubSub Subscriber executable
add_executable(pubsub_subscriber
    pubsub_subscriber.c
)
target_link_libraries(pubsub_subscriber PRIVATE open62541)
target_include_directories(pubsub_subscriber PRIVATE ${PROJECT_SOURCE_DIR}/include)
set_target_properties(pubsub_subscriber PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}/../build/bin/redundancy"
)

# ---------------------------------------------
# PubSub Publisher executable
set(CMAKE_BUILD_TYPE Debug)
add_executable(pubsub_publisher
    pubsub_publisher.c
    ${REDUNDANCY_SRC}
)
target_link_libraries(pubsub_publisher PRIVATE open62541)
target_include_directories(pubsub_publisher PRIVATE ${PROJECT_SOURCE_DIR}/include)
set_target_properties(pubsub_publisher PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}/../build/bin/redundancy"
)
