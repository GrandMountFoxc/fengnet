# 查找./src目录下的所有源文件，保存到DIR_SRCS变量
aux_source_directory(${CMAKE_SOURCE_DIR}/fengnet-src DIR_SRCS)
# 指定生成可执行文件
add_executable(Fengnet ${DIR_SRCS})
# 库文件
find_package (Threads)
target_link_libraries (Fengnet ${CMAKE_THREAD_LIBS_INIT})
# lua头文件、库
# include_directories(${CMAKE_SOURCE_DIR}/3rd/lib)
# target_link_libraries(Fengnet ${CMAKE_SOURCE_DIR}/3rd/lib/liblua.a)
include_directories(/usr/local/lib)
target_link_libraries(Fengnet /usr/local/lib/liblua.a)
target_link_libraries(Fengnet dl)
# 动态库
link_directories(${CMAKE_SOURCE_DIR}/cservice)
target_link_libraries(Fengnet ${CMAKE_SOURCE_DIR}/cservice/liblogger.so)    # /usr/bin/ld: cannot find -llogger
target_link_libraries(Fengnet ${CMAKE_SOURCE_DIR}/cservice/libsnlua.so)

# 链接luaclib动态库
link_directories(${CMAKE_SOURCE_DIR}/luaclib)
target_link_libraries(Fengnet ${CMAKE_SOURCE_DIR}/luaclib/libbson.so)    # /usr/bin/ld: cannot find -llogger
target_link_libraries(Fengnet ${CMAKE_SOURCE_DIR}/luaclib/libclient.so)
target_link_libraries(Fengnet ${CMAKE_SOURCE_DIR}/luaclib/liblpeg.so)
target_link_libraries(Fengnet ${CMAKE_SOURCE_DIR}/luaclib/libmd5.so)
target_link_libraries(Fengnet ${CMAKE_SOURCE_DIR}/luaclib/libfengnet.so)
target_link_libraries(Fengnet ${CMAKE_SOURCE_DIR}/luaclib/libsproto.so)

# add_library(bson SHARED IMPORTED)
# add_library(client SHARED IMPORTED)
# add_library(lpeg SHARED IMPORTED)
# add_library(md5 SHARED IMPORTED)
# add_library(skynet SHARED IMPORTED)
# add_library(sproto SHARED IMPORTED)
# set_target_properties(bson PROPERTIES IMPORTED_LOCATION ${CMAKE_SOURCE_DIR}/luaclib/bson.so)
# set_target_properties(client PROPERTIES IMPORTED_LOCATION ${CMAKE_SOURCE_DIR}/luaclib/client.so)
# set_target_properties(lpeg PROPERTIES IMPORTED_LOCATION ${CMAKE_SOURCE_DIR}/luaclib/lpeg.so)
# set_target_properties(md5 PROPERTIES IMPORTED_LOCATION ${CMAKE_SOURCE_DIR}/luaclib/md5.so)
# set_target_properties(skynet PROPERTIES IMPORTED_LOCATION ${CMAKE_SOURCE_DIR}/luaclib/skynet.so)
# set_target_properties(sproto PROPERTIES IMPORTED_LOCATION ${CMAKE_SOURCE_DIR}/luaclib/sproto.so)
# target_link_libraries(fengnet bson)
# target_link_libraries(fengnet client)
# target_link_libraries(fengnet lpeg)
# target_link_libraries(fengnet md5)
# target_link_libraries(fengnet skynet)
# target_link_libraries(fengnet sproto)