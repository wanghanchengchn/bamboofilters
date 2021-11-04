add_library(header INTERFACE)
target_include_directories(header INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

# hash function
add_library(hash STATIC "common/BOBHash.cpp")
target_link_libraries(hash PUBLIC header)
