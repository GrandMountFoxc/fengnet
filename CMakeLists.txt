# 项目名称
project (fengnet)
# CMake最低版本号要求
cmake_minimum_required (VERSION 2.8)
# 头文件目录
include_directories(include)
# 查找./src目录下的所有源文件，保存到DIR_SRCS变量
aux_source_directory(./fengnet-src DIR_SRCS)
# 添加库文件路径
link_directories(${CMAKE_CURRENT_SOURCE_DIR}/3rd/lua-5.4.6/src/)
link_directories(${CMAKE_CURRENT_SOURCE_DIR}/3rd/jemalloc/lib/)
# gdb调试
SET(CMAKE_BUILD_TYPE "Debug")
SET(CMAKE_CXX_FLAGS_DEBUG "$ENV{CXXFLAGS} -O0 -g2 -ggdb")
SET(CMAKE_CXX_FLAGS_RELEASE "$ENV{CXXFLAGS} -O3 -Wall")
# 用C++11
add_definitions(-std=c++17)
# 指定生成目标文件
add_executable(fengnet ${DIR_SRCS})
# 库文件
find_package (Threads)
target_link_libraries (fengnet ${CMAKE_THREAD_LIBS_INIT})
# lua头文件、库
include_directories(./3rd/lua-5.4.6/src)
target_link_libraries(fengnet liblua.a)
target_link_libraries(fengnet dl)
# jemalloc头文件、库
# include_directories(./3rd/jemalloc/lib)
# target_link_libraries(fengnet libjemalloc.a)
# 动态库
link_directories(${PROJECT_SOURCE_DIR}/cservice)
target_link_libraries(fengnet ${PROJECT_SOURCE_DIR}/cservice/liblogger.so)    # /usr/bin/ld: cannot find -llogger
target_link_libraries(fengnet ${PROJECT_SOURCE_DIR}/cservice/libsnlua.so)