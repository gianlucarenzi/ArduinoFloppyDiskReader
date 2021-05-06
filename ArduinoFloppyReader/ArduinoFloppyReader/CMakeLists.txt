# CMakeLists file
set (SOURCES Main.cpp)
set (EXEC avrfloppyreader)
if(MINGW OR MSYS)
	# Add specific Windows Files
	set (SOURCES ${SOURCES} stdafx.cpp)
	set (EXEC avrfloppy.exe)
endif()

message (STATUS "Source Code for library: ${SOURCES}")
message (STATUS "Executable: ${EXEC}")

add_executable (${EXEC} ${SOURCES})

# Make sure the compiler can find include files for our lib library
# when other libraries or executables link to lib
target_link_libraries (${EXEC} LINK_PUBLIC afdr)
