# CMAKE generated file: DO NOT EDIT!
# Generated by "Unix Makefiles" Generator, CMake Version 3.10

# Delete rule output on recipe failure.
.DELETE_ON_ERROR:


#=============================================================================
# Special targets provided by cmake.

# Disable implicit rules so canonical targets will work.
.SUFFIXES:


# Remove some rules from gmake that .SUFFIXES does not remove.
SUFFIXES =

.SUFFIXES: .hpux_make_needs_suffix_list


# Suppress display of executed commands.
$(VERBOSE).SILENT:


# A target that is always out of date.
cmake_force:

.PHONY : cmake_force

#=============================================================================
# Set environment variables for the build.

# The shell in which to execute make rules.
SHELL = /bin/sh

# The CMake executable.
CMAKE_COMMAND = /usr/bin/cmake

# The command to remove a file.
RM = /usr/bin/cmake -E remove -f

# Escaping for special characters.
EQUALS = =

# The top-level source directory on which CMake was run.
CMAKE_SOURCE_DIR = /home/ubuntu/fengnet/lualib-src

# The top-level build directory on which CMake was run.
CMAKE_BINARY_DIR = /home/ubuntu/fengnet/lualib-src/build

# Include any dependencies generated for this target.
include CMakeFiles/client.dir/depend.make

# Include the progress variables for this target.
include CMakeFiles/client.dir/progress.make

# Include the compile flags for this target's objects.
include CMakeFiles/client.dir/flags.make

CMakeFiles/client.dir/lua-clientsocket.cpp.o: CMakeFiles/client.dir/flags.make
CMakeFiles/client.dir/lua-clientsocket.cpp.o: ../lua-clientsocket.cpp
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/home/ubuntu/fengnet/lualib-src/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_1) "Building CXX object CMakeFiles/client.dir/lua-clientsocket.cpp.o"
	/usr/bin/c++  $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -o CMakeFiles/client.dir/lua-clientsocket.cpp.o -c /home/ubuntu/fengnet/lualib-src/lua-clientsocket.cpp

CMakeFiles/client.dir/lua-clientsocket.cpp.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/client.dir/lua-clientsocket.cpp.i"
	/usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -E /home/ubuntu/fengnet/lualib-src/lua-clientsocket.cpp > CMakeFiles/client.dir/lua-clientsocket.cpp.i

CMakeFiles/client.dir/lua-clientsocket.cpp.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/client.dir/lua-clientsocket.cpp.s"
	/usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -S /home/ubuntu/fengnet/lualib-src/lua-clientsocket.cpp -o CMakeFiles/client.dir/lua-clientsocket.cpp.s

CMakeFiles/client.dir/lua-clientsocket.cpp.o.requires:

.PHONY : CMakeFiles/client.dir/lua-clientsocket.cpp.o.requires

CMakeFiles/client.dir/lua-clientsocket.cpp.o.provides: CMakeFiles/client.dir/lua-clientsocket.cpp.o.requires
	$(MAKE) -f CMakeFiles/client.dir/build.make CMakeFiles/client.dir/lua-clientsocket.cpp.o.provides.build
.PHONY : CMakeFiles/client.dir/lua-clientsocket.cpp.o.provides

CMakeFiles/client.dir/lua-clientsocket.cpp.o.provides.build: CMakeFiles/client.dir/lua-clientsocket.cpp.o


CMakeFiles/client.dir/lua-crypt.cpp.o: CMakeFiles/client.dir/flags.make
CMakeFiles/client.dir/lua-crypt.cpp.o: ../lua-crypt.cpp
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/home/ubuntu/fengnet/lualib-src/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_2) "Building CXX object CMakeFiles/client.dir/lua-crypt.cpp.o"
	/usr/bin/c++  $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -o CMakeFiles/client.dir/lua-crypt.cpp.o -c /home/ubuntu/fengnet/lualib-src/lua-crypt.cpp

CMakeFiles/client.dir/lua-crypt.cpp.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/client.dir/lua-crypt.cpp.i"
	/usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -E /home/ubuntu/fengnet/lualib-src/lua-crypt.cpp > CMakeFiles/client.dir/lua-crypt.cpp.i

CMakeFiles/client.dir/lua-crypt.cpp.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/client.dir/lua-crypt.cpp.s"
	/usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -S /home/ubuntu/fengnet/lualib-src/lua-crypt.cpp -o CMakeFiles/client.dir/lua-crypt.cpp.s

CMakeFiles/client.dir/lua-crypt.cpp.o.requires:

.PHONY : CMakeFiles/client.dir/lua-crypt.cpp.o.requires

CMakeFiles/client.dir/lua-crypt.cpp.o.provides: CMakeFiles/client.dir/lua-crypt.cpp.o.requires
	$(MAKE) -f CMakeFiles/client.dir/build.make CMakeFiles/client.dir/lua-crypt.cpp.o.provides.build
.PHONY : CMakeFiles/client.dir/lua-crypt.cpp.o.provides

CMakeFiles/client.dir/lua-crypt.cpp.o.provides.build: CMakeFiles/client.dir/lua-crypt.cpp.o


CMakeFiles/client.dir/lsha1.cpp.o: CMakeFiles/client.dir/flags.make
CMakeFiles/client.dir/lsha1.cpp.o: ../lsha1.cpp
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/home/ubuntu/fengnet/lualib-src/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_3) "Building CXX object CMakeFiles/client.dir/lsha1.cpp.o"
	/usr/bin/c++  $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -o CMakeFiles/client.dir/lsha1.cpp.o -c /home/ubuntu/fengnet/lualib-src/lsha1.cpp

CMakeFiles/client.dir/lsha1.cpp.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/client.dir/lsha1.cpp.i"
	/usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -E /home/ubuntu/fengnet/lualib-src/lsha1.cpp > CMakeFiles/client.dir/lsha1.cpp.i

CMakeFiles/client.dir/lsha1.cpp.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/client.dir/lsha1.cpp.s"
	/usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -S /home/ubuntu/fengnet/lualib-src/lsha1.cpp -o CMakeFiles/client.dir/lsha1.cpp.s

CMakeFiles/client.dir/lsha1.cpp.o.requires:

.PHONY : CMakeFiles/client.dir/lsha1.cpp.o.requires

CMakeFiles/client.dir/lsha1.cpp.o.provides: CMakeFiles/client.dir/lsha1.cpp.o.requires
	$(MAKE) -f CMakeFiles/client.dir/build.make CMakeFiles/client.dir/lsha1.cpp.o.provides.build
.PHONY : CMakeFiles/client.dir/lsha1.cpp.o.provides

CMakeFiles/client.dir/lsha1.cpp.o.provides.build: CMakeFiles/client.dir/lsha1.cpp.o


# Object files for target client
client_OBJECTS = \
"CMakeFiles/client.dir/lua-clientsocket.cpp.o" \
"CMakeFiles/client.dir/lua-crypt.cpp.o" \
"CMakeFiles/client.dir/lsha1.cpp.o"

# External object files for target client
client_EXTERNAL_OBJECTS =

../luaclib/libclient.so: CMakeFiles/client.dir/lua-clientsocket.cpp.o
../luaclib/libclient.so: CMakeFiles/client.dir/lua-crypt.cpp.o
../luaclib/libclient.so: CMakeFiles/client.dir/lsha1.cpp.o
../luaclib/libclient.so: CMakeFiles/client.dir/build.make
../luaclib/libclient.so: CMakeFiles/client.dir/link.txt
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --bold --progress-dir=/home/ubuntu/fengnet/lualib-src/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_4) "Linking CXX shared library ../luaclib/libclient.so"
	$(CMAKE_COMMAND) -E cmake_link_script CMakeFiles/client.dir/link.txt --verbose=$(VERBOSE)

# Rule to build all files generated by this target.
CMakeFiles/client.dir/build: ../luaclib/libclient.so

.PHONY : CMakeFiles/client.dir/build

CMakeFiles/client.dir/requires: CMakeFiles/client.dir/lua-clientsocket.cpp.o.requires
CMakeFiles/client.dir/requires: CMakeFiles/client.dir/lua-crypt.cpp.o.requires
CMakeFiles/client.dir/requires: CMakeFiles/client.dir/lsha1.cpp.o.requires

.PHONY : CMakeFiles/client.dir/requires

CMakeFiles/client.dir/clean:
	$(CMAKE_COMMAND) -P CMakeFiles/client.dir/cmake_clean.cmake
.PHONY : CMakeFiles/client.dir/clean

CMakeFiles/client.dir/depend:
	cd /home/ubuntu/fengnet/lualib-src/build && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" /home/ubuntu/fengnet/lualib-src /home/ubuntu/fengnet/lualib-src /home/ubuntu/fengnet/lualib-src/build /home/ubuntu/fengnet/lualib-src/build /home/ubuntu/fengnet/lualib-src/build/CMakeFiles/client.dir/DependInfo.cmake --color=$(COLOR)
.PHONY : CMakeFiles/client.dir/depend
