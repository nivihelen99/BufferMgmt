cmake_minimum_required(VERSION 3.15)
project(PacketBufferManager CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Define the library
add_library(packetbuffer src/packet_buffer.cpp src/packet_buffer_pool.cpp src/buffer_metadata.cpp src/pool_manager.cpp)

# Specify include directories for the library
target_include_directories(packetbuffer PUBLIC include)

# Enable testing with CTest
enable_testing()

# Example for adding GoogleTest (assuming it's found by CMake)
# find_package(GTest REQUIRED)
# if(GTEST_FOUND)
#   add_executable(run_tests tests/main_test.cpp tests/packet_buffer_test.cpp)
#   target_link_libraries(run_tests GTest::GTest GTest::Main packetbuffer)
#   add_test(NAME UnitTests COMMAND run_tests)
# endif()

install(TARGETS packetbuffer
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
)

install(DIRECTORY include/ DESTINATION include)

# Generate pkg-config file
include(GNUInstallDirs)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/packetbuffermanager.pc DESTINATION ${CMAKE_INSTALL_DATADIR}/pkgconfig)

# --- Generate pkg-config file ---
set(prefix "${CMAKE_INSTALL_PREFIX}")
set(exec_prefix "${prefix}")
set(libdir "${CMAKE_INSTALL_LIBDIR}")
set(includedir "${CMAKE_INSTALL_INCLUDEDIR}")
set(PACKAGE_NAME "PacketBufferManager")
set(PACKAGE_VERSION "0.1.0") # You can set your package version
set(PACKAGE_DESCRIPTION "Network Packet Buffer Pool Manager Library")

# Configure the .pc file
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/packetbuffermanager.pc.in"
    "${CMAKE_CURRENT_BINARY_DIR}/packetbuffermanager.pc"
    @ONLY
)

# Install the .pc file
install(
    FILES "${CMAKE_CURRENT_BINARY_DIR}/packetbuffermanager.pc"
    DESTINATION "${CMAKE_INSTALL_DATADIR}/pkgconfig"
)

# --- GoogleTest Setup ---
enable_testing()
find_package(GTest REQUIRED)

# Create a directory for tests if it doesn't exist (CMake will create it for add_executable sources)
# file(MAKE_DIRECTORY ${CMAKE_SOURCE_DIR}/tests) # Not strictly needed if files are listed below

add_executable(run_tests
    tests/main_test.cpp
    tests/packet_buffer_test.cpp
    tests/buffer_metadata_test.cpp
    tests/packet_buffer_pool_test.cpp
    tests/pool_manager_test.cpp
)

target_link_libraries(run_tests
    PRIVATE GTest::GTest GTest::Main packetbuffer
)

include(GoogleTest)
gtest_discover_tests(run_tests)
# --- End GoogleTest Setup ---
