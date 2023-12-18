# 查找./src目录下的所有源文件，保存到DIR_SRCS变量
aux_source_directory(${CMAKE_SOURCE_DIR}/fengnet-src DIR_SRCS)
# 添加库文件路径
link_directories(${CMAKE_SOURCE_DIR}/3rd/lua-5.4.6/src/)
link_directories(${CMAKE_SOURCE_DIR}/3rd/jemalloc/lib/)
# 指定生成目标文件
add_executable(fengnet ${DIR_SRCS})
# 库文件
find_package (Threads)
target_link_libraries (fengnet ${CMAKE_THREAD_LIBS_INIT})
# lua头文件、库
include_directories(./3rd/lua-5.4.6/src)
target_link_libraries(fengnet liblua.a)
target_link_libraries(fengnet dl)
# 动态库
link_directories(${CMAKE_SOURCE_DIR}/cservice)
target_link_libraries(fengnet ${CMAKE_SOURCE_DIR}/cservice/liblogger.so)    # /usr/bin/ld: cannot find -llogger
target_link_libraries(fengnet ${CMAKE_SOURCE_DIR}/cservice/libsnlua.so)