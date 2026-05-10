# Veyon Policy Agent - standalone systemd service.
#
# Independent of veyon-server / veyon-service. Runs as a regular
# Linux daemon and pulls policy from the central control server.

cmake_minimum_required(VERSION 3.16)

# Find Qt - prefer Qt6, fall back to Qt5
if(NOT TARGET Qt${QT_MAJOR_VERSION}::Core)
    find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core Network)
    find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Network)
    set(QT_MAJOR_VERSION ${QT_VERSION_MAJOR})
endif()

set(CMAKE_AUTOMOC ON)

add_executable(veyon-policy-agent
    main.cpp
    PolicyAgent.cpp
    PolicyAgent.h
    HostsWriter.cpp
    HostsWriter.h
    ConfigLoader.cpp
    ConfigLoader.h
    # Reuse HttpClient from the centralpolicy plugin - same code,
    # no duplication.
    ${CMAKE_SOURCE_DIR}/plugins/centralpolicy/HttpClient.cpp
    ${CMAKE_SOURCE_DIR}/plugins/centralpolicy/HttpClient.h
)

target_include_directories(veyon-policy-agent PRIVATE
    ${CMAKE_SOURCE_DIR}/plugins/centralpolicy
)

target_link_libraries(veyon-policy-agent PRIVATE
    Qt${QT_MAJOR_VERSION}::Core
    Qt${QT_MAJOR_VERSION}::Network
)

# Don't include in the main Veyon .deb - the agent has its own
# installer (install.sh) and its own systemd unit.
set_target_properties(veyon-policy-agent PROPERTIES
    EXCLUDE_FROM_ALL FALSE
)
