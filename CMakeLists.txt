cmake_minimum_required(VERSION 3.15)
project(OpenGLProject C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(include)
link_directories(lib)

add_executable(main src/main.cpp src/glad.c)

target_link_libraries(main PRIVATE
    glfw3
    opengl32
    gdi32
)
