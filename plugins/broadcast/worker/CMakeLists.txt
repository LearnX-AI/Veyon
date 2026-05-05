add_executable(veyon-broadcast-worker main.cpp)

target_link_libraries(veyon-broadcast-worker PRIVATE
    Qt${QT_MAJOR_VERSION}::Core
    Qt${QT_MAJOR_VERSION}::Gui
    Qt${QT_MAJOR_VERSION}::Widgets
)

install(TARGETS veyon-broadcast-worker
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
