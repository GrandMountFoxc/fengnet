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
CMAKE_SOURCE_DIR = /home/ubuntu/fengnet

# The top-level build directory on which CMake was run.
CMAKE_BINARY_DIR = /home/ubuntu/fengnet/build

# Include any dependencies generated for this target.
include service-src/CMakeFiles/logger.dir/depend.make

# Include the progress variables for this target.
include service-src/CMakeFiles/logger.dir/progress.make

# Include the compile flags for this target's objects.
include service-src/CMakeFiles/logger.dir/flags.make

service-src/CMakeFiles/logger.dir/logger/service_logger.cpp.o: service-src/CMakeFiles/logger.dir/flags.make
service-src/CMakeFiles/logger.dir/logger/service_logger.cpp.o: ../service-src/logger/service_logger.cpp
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/home/ubuntu/fengnet/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_1) "Building CXX object service-src/CMakeFiles/logger.dir/logger/service_logger.cpp.o"
	cd /home/ubuntu/fengnet/build/service-src && /usr/bin/c++  $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -o CMakeFiles/logger.dir/logger/service_logger.cpp.o -c /home/ubuntu/fengnet/service-src/logger/service_logger.cpp

service-src/CMakeFiles/logger.dir/logger/service_logger.cpp.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/logger.dir/logger/service_logger.cpp.i"
	cd /home/ubuntu/fengnet/build/service-src && /usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -E /home/ubuntu/fengnet/service-src/logger/service_logger.cpp > CMakeFiles/logger.dir/logger/service_logger.cpp.i

service-src/CMakeFiles/logger.dir/logger/service_logger.cpp.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/logger.dir/logger/service_logger.cpp.s"
	cd /home/ubuntu/fengnet/build/service-src && /usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -S /home/ubuntu/fengnet/service-src/logger/service_logger.cpp -o CMakeFiles/logger.dir/logger/service_logger.cpp.s

service-src/CMakeFiles/logger.dir/logger/service_logger.cpp.o.requires:

.PHONY : service-src/CMakeFiles/logger.dir/logger/service_logger.cpp.o.requires

service-src/CMakeFiles/logger.dir/logger/service_logger.cpp.o.provides: service-src/CMakeFiles/logger.dir/logger/service_logger.cpp.o.requires
	$(MAKE) -f service-src/CMakeFiles/logger.dir/build.make service-src/CMakeFiles/logger.dir/logger/service_logger.cpp.o.provides.build
.PHONY : service-src/CMakeFiles/logger.dir/logger/service_logger.cpp.o.provides

service-src/CMakeFiles/logger.dir/logger/service_logger.cpp.o.provides.build: service-src/CMakeFiles/logger.dir/logger/service_logger.cpp.o


# Object files for target logger
logger_OBJECTS = \
"CMakeFiles/logger.dir/logger/service_logger.cpp.o"

# External object files for target logger
logger_EXTERNAL_OBJECTS =

../cservice/liblogger.so: service-src/CMakeFiles/logger.dir/logger/service_logger.cpp.o
../cservice/liblogger.so: service-src/CMakeFiles/logger.dir/build.make
../cservice/liblogger.so: service-src/CMakeFiles/logger.dir/link.txt
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --bold --progress-dir=/home/ubuntu/fengnet/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_2) "Linking CXX shared library ../../cservice/liblogger.so"
	cd /home/ubuntu/fengnet/build/service-src && $(CMAKE_COMMAND) -E cmake_link_script CMakeFiles/logger.dir/link.txt --verbose=$(VERBOSE)

# Rule to build all files generated by this target.
service-src/CMakeFiles/logger.dir/build: ../cservice/liblogger.so

.PHONY : service-src/CMakeFiles/logger.dir/build

service-src/CMakeFiles/logger.dir/requires: service-src/CMakeFiles/logger.dir/logger/service_logger.cpp.o.requires

.PHONY : service-src/CMakeFiles/logger.dir/requires

service-src/CMakeFiles/logger.dir/clean:
	cd /home/ubuntu/fengnet/build/service-src && $(CMAKE_COMMAND) -P CMakeFiles/logger.dir/cmake_clean.cmake
.PHONY : service-src/CMakeFiles/logger.dir/clean

service-src/CMakeFiles/logger.dir/depend:
	cd /home/ubuntu/fengnet/build && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" /home/ubuntu/fengnet /home/ubuntu/fengnet/service-src /home/ubuntu/fengnet/build /home/ubuntu/fengnet/build/service-src /home/ubuntu/fengnet/build/service-src/CMakeFiles/logger.dir/DependInfo.cmake --color=$(COLOR)
.PHONY : service-src/CMakeFiles/logger.dir/depend
