cmake_minimum_required(VERSION 3.15)
project(JungleRestStop C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS src/*.cpp)
add_executable(main ${SOURCES} src/glad.c)
target_include_directories(main PRIVATE include src)
target_link_directories(main PRIVATE lib)
target_link_libraries(main PRIVATE glfw3 opengl32 gdi32 winmm)
