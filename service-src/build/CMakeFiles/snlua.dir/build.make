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
CMAKE_SOURCE_DIR = /home/ubuntu/fengnet/service-src

# The top-level build directory on which CMake was run.
CMAKE_BINARY_DIR = /home/ubuntu/fengnet/service-src/build

# Include any dependencies generated for this target.
include CMakeFiles/snlua.dir/depend.make

# Include the progress variables for this target.
include CMakeFiles/snlua.dir/progress.make

# Include the compile flags for this target's objects.
include CMakeFiles/snlua.dir/flags.make

CMakeFiles/snlua.dir/snlua/service_snlua.cpp.o: CMakeFiles/snlua.dir/flags.make
CMakeFiles/snlua.dir/snlua/service_snlua.cpp.o: ../snlua/service_snlua.cpp
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/home/ubuntu/fengnet/service-src/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_1) "Building CXX object CMakeFiles/snlua.dir/snlua/service_snlua.cpp.o"
	/usr/bin/c++  $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -o CMakeFiles/snlua.dir/snlua/service_snlua.cpp.o -c /home/ubuntu/fengnet/service-src/snlua/service_snlua.cpp

CMakeFiles/snlua.dir/snlua/service_snlua.cpp.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/snlua.dir/snlua/service_snlua.cpp.i"
	/usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -E /home/ubuntu/fengnet/service-src/snlua/service_snlua.cpp > CMakeFiles/snlua.dir/snlua/service_snlua.cpp.i

CMakeFiles/snlua.dir/snlua/service_snlua.cpp.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/snlua.dir/snlua/service_snlua.cpp.s"
	/usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -S /home/ubuntu/fengnet/service-src/snlua/service_snlua.cpp -o CMakeFiles/snlua.dir/snlua/service_snlua.cpp.s

CMakeFiles/snlua.dir/snlua/service_snlua.cpp.o.requires:

.PHONY : CMakeFiles/snlua.dir/snlua/service_snlua.cpp.o.requires

CMakeFiles/snlua.dir/snlua/service_snlua.cpp.o.provides: CMakeFiles/snlua.dir/snlua/service_snlua.cpp.o.requires
	$(MAKE) -f CMakeFiles/snlua.dir/build.make CMakeFiles/snlua.dir/snlua/service_snlua.cpp.o.provides.build
.PHONY : CMakeFiles/snlua.dir/snlua/service_snlua.cpp.o.provides

CMakeFiles/snlua.dir/snlua/service_snlua.cpp.o.provides.build: CMakeFiles/snlua.dir/snlua/service_snlua.cpp.o


# Object files for target snlua
snlua_OBJECTS = \
"CMakeFiles/snlua.dir/snlua/service_snlua.cpp.o"

# External object files for target snlua
snlua_EXTERNAL_OBJECTS =

/home/ubuntu/fengnet/cservice/libsnlua.so: CMakeFiles/snlua.dir/snlua/service_snlua.cpp.o
/home/ubuntu/fengnet/cservice/libsnlua.so: CMakeFiles/snlua.dir/build.make
/home/ubuntu/fengnet/cservice/libsnlua.so: CMakeFiles/snlua.dir/link.txt
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --bold --progress-dir=/home/ubuntu/fengnet/service-src/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_2) "Linking CXX shared library /home/ubuntu/fengnet/cservice/libsnlua.so"
	$(CMAKE_COMMAND) -E cmake_link_script CMakeFiles/snlua.dir/link.txt --verbose=$(VERBOSE)

# Rule to build all files generated by this target.
CMakeFiles/snlua.dir/build: /home/ubuntu/fengnet/cservice/libsnlua.so

.PHONY : CMakeFiles/snlua.dir/build

CMakeFiles/snlua.dir/requires: CMakeFiles/snlua.dir/snlua/service_snlua.cpp.o.requires

.PHONY : CMakeFiles/snlua.dir/requires

CMakeFiles/snlua.dir/clean:
	$(CMAKE_COMMAND) -P CMakeFiles/snlua.dir/cmake_clean.cmake
.PHONY : CMakeFiles/snlua.dir/clean

CMakeFiles/snlua.dir/depend:
	cd /home/ubuntu/fengnet/service-src/build && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" /home/ubuntu/fengnet/service-src /home/ubuntu/fengnet/service-src /home/ubuntu/fengnet/service-src/build /home/ubuntu/fengnet/service-src/build /home/ubuntu/fengnet/service-src/build/CMakeFiles/snlua.dir/DependInfo.cmake --color=$(COLOR)
.PHONY : CMakeFiles/snlua.dir/depend

