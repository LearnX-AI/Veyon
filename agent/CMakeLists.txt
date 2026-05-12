# Veyon Policy Agent - standalone systemd service.
#
# Independent of veyon-server / veyon-service. Runs as a regular
# Linux daemon and pulls policy from the central control server.
# Packaged into the main veyon .deb so it auto-installs on every PC.

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
    FileDownloader.cpp
    FileDownloader.h
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

# ---- Installation ----
install(TARGETS veyon-policy-agent
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(FILES veyon-policy-agent.service.in
    DESTINATION /lib/systemd/system
    RENAME veyon-policy-agent.service
)

install(FILES config.example.conf
    DESTINATION /etc/veyon-policy-agent
)
