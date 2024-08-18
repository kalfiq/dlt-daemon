cmake_minimum_required(VERSION 3.3)

add_executable(${CMAKE_PROJECT_NAME} main.cpp)

find_package(automotive-dlt REQUIRED)

target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE Genivi::dlt)
