# 用C++17
add_definitions(-std=c++17)
# CMake最低版本号要求
cmake_minimum_required (VERSION 2.8)
# set(CMAKE_CXX_FLAGS "-fPIC")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIE")

set(PREFIX /home/ubuntu/fengnet)

# 把源文件添加到列表
include_directories(${PREFIX}/fengnet-src)
include_directories(${PREFIX}/service-src)
include_directories(${PREFIX}/lualib-src)
include_directories(${PREFIX}/3rd/lua-md5)
include_directories(${PREFIX}/3rd/lpeg)
include_directories(${PREFIX}/lualib-src/sproto)

set(FENGNET_SRCS lua-socket.h lua-mongo.h lua-netpack.h lua-memory.h
    lua-multicast.h lua-cluster.h lua-crypt.h lsha1.h
    lua-sharedata.h lua-stm.h
    lua-debugchannel.h lua-datasheet.h lua-seri.h lua-fengnet.h
    lua-socket.cpp lua-mongo.cpp lua-netpack.cpp lua-memory.cpp
    lua-multicast.cpp lua-cluster.cpp lua-crypt.cpp lsha1.cpp
    lua-sharedata.cpp lua-stm.cpp
    lua-debugchannel.cpp lua-datasheet.cpp lua-seri.cpp lua-fengnet.cpp)

set(BSON_SRCS lua-bson.cpp)

set(MD5_SRCS ${PREFIX}/3rd/lua-md5/md5.c ${PREFIX}/3rd/lua-md5/md5lib.c ${PREFIX}/3rd/lua-md5/compat-5.2.c)

set(CLIENT_SRCS lua-clientsocket.cpp lua-crypt.cpp lsha1.cpp)

set(SPROTO_SRCS sproto/sproto.c sproto/lsproto.c)

set(LPEG_SRCS ${PREFIX}/3rd/lpeg/lpcap.c ${PREFIX}/3rd/lpeg/lpcode.c ${PREFIX}/3rd/lpeg/lpprint.c 
    ${PREFIX}/3rd/lpeg/lptree.c ${PREFIX}/3rd/lpeg/lpvm.c ${PREFIX}/3rd/lpeg/lpcset.c)

aux_source_directory(/home/ubuntu/fengnet/fengnet-src ADDITION)

# 设置要链接的其他库
find_library(LUA_LIBRARY lua REQUIRED)
message(">>> value = ${PREFIX}")
# 指定编译库输出的位置
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/luaclib)
#生成动态库
add_library(fengnet SHARED ${ADDITION} ${FENGNET_SRCS})
add_library(bson SHARED ${BSON_SRCS})
add_library(md5 SHARED ${MD5_SRCS})
add_library(client SHARED ${CLIENT_SRCS})
add_library(sproto SHARED ${SPROTO_SRCS})
add_library(lpeg SHARED ${LPEG_SRCS})
# 链接其他库
target_link_libraries(fengnet ${LUA_LIBRARY})
target_link_libraries(bson ${LUA_LIBRARY})
find_package (Threads)
target_link_libraries (fengnet ${CMAKE_THREAD_LIBS_INIT})
target_link_libraries (client ${CMAKE_THREAD_LIBS_INIT})
target_link_libraries (fengnet dl)
target_link_libraries (fengnet m)

# 安装共享库和头文件
set(CMAKE_INSTALL_PREFIX /usr/local)
install(TARGETS fengnet LIBRARY DESTINATION lib)
# install(FILES ${PROJECT_INIT_PATH}/fengnet-src/fengnet.h DESTINATION include) # 把fengnet.h拷贝一份到include文件夹下面，对我来说没什么意义