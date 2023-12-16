cmake_minimum_required(VERSION 2.8.0)
# 获取当前路径的上层路径
string(REGEX REPLACE "(.*)/(.*)" "\\0" PROJECT_INIT_PATH ${PROJECT_SOURCE_DIR})
message("上层目录=" &{PROJECT_INIT_PATH})

# 把所有源文件添加到列表
aux_source_directory(./ DIR_LIB_SRCS)
# 指定编译库输出的位置
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${PROJECT_INIT_PATH}/cservice)
include_directories(${PROJECT_INIT_PATH}/fengnet-src)
#生成动态库