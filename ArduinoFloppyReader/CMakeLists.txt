# CMakeLists file

# In my Debian Buster I have v3.13.4
cmake_minimum_required (VERSION 2.8.11)

project (ArduinoFloppyDiskReader)
set (CMAKE_VERBOSE_MAKEFILE ON)

# Creating variables depending on OS

if(UNIX)
	add_compile_definitions (__LINUX__)
endif()

if(APPLE)
	add_compile_definitions (__MACOS__)
endif()

if(MINGW OR MSYS)
	add_compile_definitions (__MINGW32__)
endif()

# For every platform
add_compile_definitions (USEGUI)

# Now I have all stuff needed to have a conditional compilation
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -L/usr/local/include/ -L/usr/include -O3 -g -Wall")
include_directories(/usr/local/include/ /usr/include)
link_directories(/usr/lib)
link_directories(/usr/local/lib)

# The two subdirectories needed are /lib and /ArduinoFloppyReader
add_subdirectory (lib)
add_subdirectory (ArduinoFloppyReader)
