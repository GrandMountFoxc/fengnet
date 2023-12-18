# 把源文件添加到列表
include_directories(${CMAKE_SOURCE_DIR}/fengnet-src)
aux_source_directory(${CMAKE_SOURCE_DIR}/service-src/logger LOGGER_SRCS)
aux_source_directory(${CMAKE_SOURCE_DIR}/service-src/snlua SNLUA_SRCS)
# 指定编译库输出的位置
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/cservice)
#生成动态库
add_library(logger SHARED ${LOGGER_SRCS})
add_library(snlua SHARED ${SNLUA_SRCS})
# 安装共享库和头文件
set(CMAKE_INSTALL_PREFIX /usr/local)
install(TARGETS logger LIBRARY DESTINATION lib)
install(TARGETS snlua LIBRARY DESTINATION lib)
# install(FILES ${PROJECT_INIT_PATH}/fengnet-src/fengnet.h DESTINATION include) # 把fengnet.h拷贝一份到include文件夹下面，对我来说没什么意义