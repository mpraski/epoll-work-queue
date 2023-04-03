cmake_minimum_required(VERSION 3.16)
project(epoll-worker-qeue)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wconversion -Werror")

find_package(CURL REQUIRED)

add_executable(coordinator
        coordinator.cpp
        CurlRequest.cpp
        Server.cpp
        Client.cpp
        utils.cpp)
target_link_libraries(coordinator PUBLIC CURL::libcurl)

add_executable(worker
        worker.cpp
        CurlRequest.cpp
        utils.cpp
        Client.h)
target_link_libraries(worker PUBLIC CURL::libcurl)

