cmake_minimum_required(VERSION 4.3)
project(PhyLayer)

set(CMAKE_CXX_STANDARD 20)

find_package(glm)
find_package(glfw3)

add_subdirectory(deps/box3d)

include_directories(include)
include_directories(deps/)

add_executable(PhyLayer
        main.cpp
        deps/glad/glad.c
)

target_link_libraries(${PROJECT_NAME} PRIVATE glm::glm glfw box3d::box3d)
