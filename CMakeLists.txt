cmake_minimum_required(VERSION 3.10)
project(webserver)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_FLAGS -pthread)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

add_executable(
    ${PROJECT_NAME}
    src/main.cpp
)

target_link_libraries(
    ${PROJECT_NAME}
    log
    common-lib
    httpconn
)

add_subdirectory(src/log)
add_subdirectory(src/common-lib)
add_subdirectory(src/http)