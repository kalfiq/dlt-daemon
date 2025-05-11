cmake_minimum_required(VERSION 3.3)

add_executable(listener main.cpp)

find_package(automotive-dlt REQUIRED)

target_link_libraries(listener 
    PRIVATE Genivi::dlt
    PRIVATE PocoFoundation
)
