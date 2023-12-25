# CMake最低版本号要求
cmake_minimum_required (VERSION 2.8)
set(CMAKE_CXX_FLAGS "-fPIC")

set(src_code
        src/lapi.c src/lcode.c src/lctype.c src/ldebug.c src/ldo.c src/ldump.c src/lfunc.c src/lgc.c src/llex.c
        src/lmem.c src/lobject.c src/lopcodes.c src/lparser.c src/lstate.c src/lstring.c src/ltable.c src/ltm.c
        src/lundump.c src/lvm.c src/lzio.c)
set(src_lib
        src/lauxlib.c src/lbaselib.c src/lcorolib.c src/ldblib.c src/liolib.c src/lmathlib.c src/loadlib.c
        src/loslib.c src/lstrlib.c src/ltablib.c src/lutf8lib.c src/linit.c)
SET (HDR_LIBLUA
        src/lapi.h src/lauxlib.h src/lcode.h src/lctype.h src/ldebug.h src/ldo.h src/lfunc.h
        src/lgc.h src/llex.h src/llimits.h src/lmem.h src/lobject.h src/lopcodes.h src/lparser.h
        src/lstate.h src/lstring.h src/ltable.h src/ltm.h src/lua.h src/luaconf.h src/lualib.h
        src/lundump.h src/lvm.h src/lzio.h)

set(src_exec src/lua.c)
set(src_comp src/luac.c)

# 指定编译库输出的位置
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/3rd/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/3rd/lib)

# 安装共享库和头文件
set(CMAKE_INSTALL_PREFIX /usr/local)

add_library(liblua_static STATIC ${src_code} ${src_lib} ${HDR_LIBLUA})
target_link_libraries(liblua_static m)
target_link_libraries(liblua_static dl)
install(TARGETS liblua_static ARCHIVE DESTINATION lib)
set_target_properties(liblua_static PROPERTIES OUTPUT_NAME lua)

add_library(liblua_share SHARED ${src_code} ${src_lib} ${HDR_LIBLUA})
target_link_libraries(liblua_share m)
target_link_libraries(liblua_share dl)
install(TARGETS liblua_share LIBRARY DESTINATION lib)
set_target_properties(liblua_share PROPERTIES OUTPUT_NAME lua)

message(STATUS "finished")