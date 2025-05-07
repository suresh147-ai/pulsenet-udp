cmake_minimum_required(VERSION 3.15)
project(pulseudp VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# If we're on Windows, we have a special set of source files.
if (WIN32)
    set(SOURCES
        src/udp_addr_win.cpp
        src/udp_win.cpp
    )
else()
    set(SOURCES
        src/udp_addr_unix.cpp
        src/udp_unix.cpp
    )
endif()
add_library(pulseudp STATIC
    ${SOURCES}
    include/pulseudp/udp_addr.h
    include/pulseudp/udp.h
)

target_include_directories(pulseudp PUBLIC include)

# Build Tests
add_executable(pulseudp_test
    tests/IntegrationTest.cpp
)
target_include_directories(pulseudp_test PRIVATE include)
target_link_libraries(pulseudp_test PRIVATE pulseudp)
add_dependencies(pulseudp_test pulseudp)

add_executable(pulseudp_ccu_test
    tests/CCUTest.cpp
)
target_include_directories(pulseudp_ccu_test PRIVATE include)
target_link_libraries(pulseudp_ccu_test PRIVATE pulseudp)
add_dependencies(pulseudp_ccu_test pulseudp)

# Install target
install(TARGETS pulseudp
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib
        RUNTIME DESTINATION bin)
install(DIRECTORY include/ DESTINATION include)

# Install test target
install(TARGETS pulseudp_test
        RUNTIME DESTINATION bin
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib)
        
install(TARGETS pulseudp_ccu_test
        RUNTIME DESTINATION bin
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib)