find_package(OpenSSL REQUIRED)

add_executable(experiment experiment.cpp)
target_link_libraries(experiment PRIVATE header hash)