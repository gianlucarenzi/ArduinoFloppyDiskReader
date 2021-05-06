# CMakeLists file
add_library (afdr ArduinoInterface.cpp ADFWriter.cpp LinuxCompat.cpp)

# Link the executable to the afdr library.
target_include_directories (afdr PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
