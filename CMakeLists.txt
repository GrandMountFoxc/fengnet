# gdb调试
SET(CMAKE_BUILD_TYPE "Debug")
SET(CMAKE_CXX_FLAGS_DEBUG "$ENV{CXXFLAGS} -O0 -g2 -ggdb")
SET(CMAKE_CXX_FLAGS_RELEASE "$ENV{CXXFLAGS} -O3 -Wall")
# 用C++17
add_definitions(-std=c++17)
# CMake最低版本号要求
cmake_minimum_required (VERSION 2.8)
# 项目名称
project (fengnet)
# 设置可执行文件保存路径
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR})
# 设置条件
# option(GEN_LIBRARY "Compile sources into a library" OFF)
# message(STATUS "Compile sources into a library?${GEN_LIBRARY}")

# 添加子目录，该目录为库目录,会调用子目录的cmake文件，进行构建库
# if(GEN_LIBRARY)
#     add_subdirectory(lib)
# else()
#     add_subdirectory(src)
# endif()
add_subdirectory(service-src)
add_subdirectory(fengnet-src)