cmake_minimum_required(VERSION 3.11)
project(examples LANGUAGES C)

# Handle libevent and libvalkey
find_library(EVENT_LIBRARY event HINTS /usr/lib/x86_64-linux-gnu)
find_package(valkey REQUIRED)
find_package(valkeycluster REQUIRED)

# Executable: IPv4
add_executable(example_ipv4 example.c)
target_link_libraries(example_ipv4
  valkeycluster::valkeycluster)

# Executable: async
add_executable(example_async example_async.c)
target_link_libraries(example_async
  valkeycluster::valkeycluster
  ${EVENT_LIBRARY})

add_executable(clientside_caching_async clientside_caching_async.c)
target_link_libraries(clientside_caching_async
  valkeycluster::valkeycluster
  ${EVENT_LIBRARY})

# Executable: tls
if(ENABLE_SSL)
  find_package(valkey_ssl REQUIRED)
  find_package(valkeycluster_ssl REQUIRED)

  add_executable(example_tls example_tls.c)
  target_link_libraries(example_tls
    valkeycluster::valkeycluster
    valkeycluster::valkeycluster_ssl
    ${EVENT_LIBRARY})
endif()
