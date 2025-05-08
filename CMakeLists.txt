cmake_minimum_required(VERSION 3.15)
project(pulseudp VERSION 1.0.0 LANGUAGES CXX)

# Detect whether this is the top-level build or used via add_subdirectory / FetchContent
if (CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR)
    set(PULSEUDP_STANDALONE_BUILD ON)
else()
    set(PULSEUDP_STANDALONE_BUILD OFF)
endif()

# Compiler setup
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Portable install directories
include(GNUInstallDirs)

# Source files depending on platform
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

# Core library target
add_library(pulseudp STATIC
    ${SOURCES}
    include/pulseudp/udp_addr.h
    include/pulseudp/udp.h
)

# Create namespaced alias
add_library(pulseudp::pulseudp ALIAS pulseudp)

# Public include directory
target_include_directories(pulseudp PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

# Tests (only built in standalone mode)
if (PULSEUDP_STANDALONE_BUILD)
    add_executable(pulseudp_test
        tests/IntegrationTest.cpp
    )
    target_link_libraries(pulseudp_test PRIVATE pulseudp)

    add_executable(pulseudp_ccu_test
        tests/CCUTest.cpp
    )
    target_link_libraries(pulseudp_ccu_test PRIVATE pulseudp)
endif()

# Export targets for installation
include(CMakePackageConfigHelpers)

install(TARGETS pulseudp
        EXPORT pulseudpTargets
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(DIRECTORY include/
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

# Export targets file
install(EXPORT pulseudpTargets
        FILE pulseudpTargets.cmake
        NAMESPACE pulseudp::
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/pulseudp
)

# Generate and install config files
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/pulseudpConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

configure_package_config_file(
    "${CMAKE_CURRENT_LIST_DIR}/cmake/pulseudpConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/pulseudpConfig.cmake"
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/pulseudp
)

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/pulseudpConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/pulseudpConfigVersion.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/pulseudp
)

# Install tests only in standalone builds
if (PULSEUDP_STANDALONE_BUILD)
    install(TARGETS pulseudp_test
            RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
            LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
            ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})

    install(TARGETS pulseudp_ccu_test
            RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
            LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
            ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
endif()
